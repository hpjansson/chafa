/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2021 Hans Petter Jansson
 *
 * This file is part of Chafa, a program that turns images into character art.
 *
 * Chafa is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Chafa is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Chafa.  If not, see <http://www.gnu.org/licenses/>. */

#include "config.h"

#include "chafa.h"
#include "smolscale/smolscale.h"
#include "internal/chafa-base64.h"
#include "internal/chafa-batch.h"
#include "internal/chafa-bitfield.h"
#include "internal/chafa-indexed-image.h"
#include "internal/chafa-iterm2-canvas.h"
#include "internal/chafa-string-util.h"

typedef enum
{
    TIFF_TYPE_NONE = 0,
    TIFF_TYPE_BYTE,
    TIFF_TYPE_ASCII,
    TIFF_TYPE_SHORT,
    TIFF_TYPE_LONG,
    TIFF_TYPE_RATIONAL,
    TIFF_TYPE_SBYTE,
    TIFF_TYPE_UNDEF,
    TIFF_TYPE_SSHORT,
    TIFF_TYPE_SLONG,
    TIFF_TYPE_SRATIONAL,
    TIFF_TYPE_FLOAT,
    TIFF_TYPE_DOUBLE
}
TiffType;

typedef enum
{
    TIFF_TAG_NONE = 0,
    TIFF_TAG_NEW_SUB_FILE_TYPE = 254,
    TIFF_TAG_SUB_FILE_TYPE,
    TIFF_TAG_IMAGE_WIDTH,
    TIFF_TAG_IMAGE_LENGTH,
    TIFF_TAG_BITS_PER_SAMPLE,
    TIFF_TAG_COMPRESSION,
    TIFF_TAG_PHOTOMETRIC_INTERPRETATION = 262,
    TIFF_TAG_MAKE = 271,
    TIFF_TAG_MODEL,
    TIFF_TAG_STRIP_OFFSETS,
    TIFF_TAG_ORIENTATION,
    TIFF_TAG_SAMPLES_PER_PIXEL = 277,
    TIFF_TAG_ROWS_PER_STRIP,
    TIFF_TAG_STRIP_BYTE_COUNTS,
    TIFF_TAG_MIN_SAMPLE_VALUE,
    TIFF_TAG_MAX_SAMPLE_VALUE,
    TIFF_TAG_X_RESOLUTION,
    TIFF_TAG_Y_RESOLUTION,
    TIFF_TAG_PLANAR_CONFIGURATION,
    TIFF_TAG_EXTRA_SAMPLES = 338
}
TiffTagId;

#define TIFF_PHOTOMETRIC_INTERPRETATION_RGB 2
#define TIFF_ORIENTATION_TOPLEFT 1
#define TIFF_PLANAR_CONFIGURATION_CONTIGUOUS 1
#define TIFF_EXTRA_SAMPLE_ASSOC_ALPHA 1

typedef struct
{
    guint16 tag_id;
    guint16 type;
    guint32 count;
    guint32 data;
}
TiffTag;

typedef struct
{
    ChafaIterm2Canvas *iterm2_canvas;
    GString *out_str;
}
BuildCtx;

typedef struct
{
    ChafaIterm2Canvas *iterm2_canvas;
    SmolScaleCtx *scale_ctx;
}
DrawCtx;

ChafaIterm2Canvas *
chafa_iterm2_canvas_new (gint width, gint height)
{
    ChafaIterm2Canvas *iterm2_canvas;

    iterm2_canvas = g_new (ChafaIterm2Canvas, 1);
    iterm2_canvas->width = width;
    iterm2_canvas->height = height;
    iterm2_canvas->rgba_image = g_malloc (width * height * sizeof (guint32));

    return iterm2_canvas;
}

void
chafa_iterm2_canvas_destroy (ChafaIterm2Canvas *iterm2_canvas)
{
    g_free (iterm2_canvas->rgba_image);
    g_free (iterm2_canvas);
}

static void
draw_pixels_worker (ChafaBatchInfo *batch, const DrawCtx *ctx)
{
    smol_scale_batch_full (ctx->scale_ctx,
                           ((guint32 *) ctx->iterm2_canvas->rgba_image) + (ctx->iterm2_canvas->width * batch->first_row),
                           batch->first_row,
                           batch->n_rows);
}

void
chafa_iterm2_canvas_draw_all_pixels (ChafaIterm2Canvas *iterm2_canvas, ChafaPixelType src_pixel_type,
                                    gconstpointer src_pixels,
                                    gint src_width, gint src_height, gint src_rowstride)
{
    DrawCtx ctx;

    g_return_if_fail (iterm2_canvas != NULL);
    g_return_if_fail (src_pixel_type < CHAFA_PIXEL_MAX);
    g_return_if_fail (src_pixels != NULL);
    g_return_if_fail (src_width >= 0);
    g_return_if_fail (src_height >= 0);

    if (src_width == 0 || src_height == 0)
        return;

    ctx.iterm2_canvas = iterm2_canvas;
    ctx.scale_ctx = smol_scale_new_full ((SmolPixelType) src_pixel_type,
                                         (const guint32 *) src_pixels,
                                         src_width,
                                         src_height,
                                         src_rowstride,
                                         SMOL_PIXEL_RGBA8_PREMULTIPLIED,
                                         NULL,
                                         iterm2_canvas->width,
                                         iterm2_canvas->height,
                                         iterm2_canvas->width * sizeof (guint32),
                                         NULL,
                                         &ctx);

    chafa_process_batches (&ctx,
                           (GFunc) draw_pixels_worker,
                           NULL,
                           iterm2_canvas->height,
                           g_get_num_processors (),
                           1);
}

static void
encode_tag (ChafaBase64 *base64, GString *gs, const TiffTag *tag)
{
    chafa_base64_encode (base64, gs, tag, sizeof (*tag));
}

static void
generate_tag (ChafaBase64 *base64, GString *gs,
              guint16 tag_id, guint16 type, guint32 count, guint32 data)
{
    TiffTag tag;

    tag.tag_id = GUINT16_TO_LE (tag_id);
    tag.type = GUINT16_TO_LE (type);
    tag.count = GUINT32_TO_LE (count);
    tag.data = GUINT32_TO_LE (data);

    encode_tag (base64, gs, &tag);
}

void
chafa_iterm2_canvas_build_ansi (ChafaIterm2Canvas *iterm2_canvas, ChafaTermInfo *term_info, GString *out_str, gint width_cells, gint height_cells)
{
    gchar seq [CHAFA_TERM_SEQ_LENGTH_MAX + 1];
    ChafaBase64 base64;
    guint32 u32;
    guint16 u16;

    *chafa_term_info_emit_begin_iterm2_image (term_info, seq, width_cells, height_cells) = '\0';
    g_string_append (out_str, seq);

    chafa_base64_init (&base64);

    /* Header and directory offset */

    u32 = GUINT32_TO_LE (0x002a4949);
    chafa_base64_encode (&base64, out_str, &u32, sizeof (u32));
    u32 = GUINT32_TO_LE (iterm2_canvas->width * iterm2_canvas->height * sizeof (guint32)
                         + sizeof (guint32) * 2);
    chafa_base64_encode (&base64, out_str, &u32, sizeof (u32));

    /* Image data */

    chafa_base64_encode (&base64, out_str,
                         iterm2_canvas->rgba_image,
                         iterm2_canvas->width * iterm2_canvas->height * sizeof (guint32));

    /* IFD */

    u16 = GUINT16_TO_LE (11);  /* Tag count */
    chafa_base64_encode (&base64, out_str, &u16, sizeof (u16));

    /* Tags */

    generate_tag (&base64, out_str, TIFF_TAG_IMAGE_WIDTH, TIFF_TYPE_LONG, 1, iterm2_canvas->width);
    generate_tag (&base64, out_str, TIFF_TAG_IMAGE_LENGTH, TIFF_TYPE_LONG, 1, iterm2_canvas->height);
    generate_tag (&base64, out_str, TIFF_TAG_BITS_PER_SAMPLE, TIFF_TYPE_SHORT, 1, 8);
    generate_tag (&base64, out_str, TIFF_TAG_PHOTOMETRIC_INTERPRETATION, TIFF_TYPE_SHORT, 1, TIFF_PHOTOMETRIC_INTERPRETATION_RGB);
    generate_tag (&base64, out_str, TIFF_TAG_STRIP_OFFSETS, TIFF_TYPE_LONG, 1, sizeof (guint32) * 2);
    generate_tag (&base64, out_str, TIFF_TAG_ORIENTATION, TIFF_TYPE_SHORT, 1, TIFF_ORIENTATION_TOPLEFT);
    generate_tag (&base64, out_str, TIFF_TAG_SAMPLES_PER_PIXEL, TIFF_TYPE_SHORT, 1, 4);
    generate_tag (&base64, out_str, TIFF_TAG_ROWS_PER_STRIP, TIFF_TYPE_LONG, 1, iterm2_canvas->height);
    generate_tag (&base64, out_str, TIFF_TAG_STRIP_BYTE_COUNTS, TIFF_TYPE_LONG, 1,
                  iterm2_canvas->width * iterm2_canvas->height * 4);
    generate_tag (&base64, out_str, TIFF_TAG_PLANAR_CONFIGURATION, TIFF_TYPE_SHORT, 1, TIFF_PLANAR_CONFIGURATION_CONTIGUOUS);
    generate_tag (&base64, out_str, TIFF_TAG_EXTRA_SAMPLES, TIFF_TYPE_SHORT, 1, TIFF_EXTRA_SAMPLE_ASSOC_ALPHA);

    /* Next IFD offset (terminator) */

    u32 = GUINT32_TO_LE (0);
    chafa_base64_encode (&base64, out_str, &u32, sizeof (u32));

    chafa_base64_encode_end (&base64, out_str);
    chafa_base64_deinit (&base64);

    *chafa_term_info_emit_end_iterm2_image (term_info, seq) = '\0';
    g_string_append (out_str, seq);
}
