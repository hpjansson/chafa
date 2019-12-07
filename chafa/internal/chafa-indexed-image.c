/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2019 Hans Petter Jansson
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

#include "smolscale/smolscale.h"
#include "chafa.h"
#include "internal/chafa-batch.h"
#include "internal/chafa-private.h"

typedef struct
{
    ChafaIndexedImage *indexed_image;
    ChafaColorSpace color_space;
    ChafaPixelType src_pixel_type;
    gconstpointer src_pixels;
    gint src_width, src_height, src_rowstride;
    gint dest_width, dest_height;

    SmolScaleCtx *scale_ctx;
    guint32 *scaled_data;
}
DrawPixelsCtx;

typedef struct
{
    ChafaSixelCanvas *sixel_canvas;
    GString *out_str;
}
BuildSixelsCtx;

static void
draw_pixels_pass_1_worker (ChafaBatchInfo *batch, const DrawPixelsCtx *ctx)
{
    smol_scale_batch_full (ctx->scale_ctx,
                           ctx->scaled_data + (ctx->dest_width * batch->first_row),
                           batch->first_row,
                           batch->n_rows);
}

static gint
quantize_pixel (const DrawPixelsCtx *ctx, ChafaColorHash *color_hash, ChafaColor color)
{
    ChafaColor cached_color;
    gint index;

    if ((gint) (color.ch [3]) < chafa_palette_get_alpha_threshold (&ctx->indexed_image->palette))
        return chafa_palette_get_transparent_index (&ctx->indexed_image->palette);

    /* Sixel color resolution is only slightly less than 7 bits per channel,
     * so eliminate the low-order bits to get better hash performance. Also
     * mask out the alpha channel. */
    CHAFA_COLOR8_U32 (cached_color) = CHAFA_COLOR8_U32 (color) & GUINT32_FROM_BE (0xfefefe00);

    index = chafa_color_hash_lookup (color_hash, CHAFA_COLOR8_U32 (cached_color));

    if (index < 0)
    {
        if (ctx->color_space == CHAFA_COLOR_SPACE_DIN99D)
            chafa_color_rgb_to_din99d (&color, &color);

        index = chafa_palette_lookup_nearest (&ctx->indexed_image->palette,
                                              ctx->color_space,
                                              &color,
                                              NULL)
          - chafa_palette_get_first_color (&ctx->indexed_image->palette);

        /* Don't insert transparent pixels, since color hash does not store transparency */
        if (index != chafa_palette_get_transparent_index (&ctx->indexed_image->palette))
            chafa_color_hash_replace (color_hash, CHAFA_COLOR8_U32 (cached_color), index);
    }

    return index;
}

static void
draw_pixels_pass_2_worker (ChafaBatchInfo *batch, const DrawPixelsCtx *ctx)
{
    const guint32 *src_p;
    guint8 *dest_p, *dest_end_p;
    ChafaColorHash chash;

    chafa_color_hash_init (&chash);

    src_p = ctx->scaled_data + (ctx->dest_width * batch->first_row);
    dest_p = ctx->indexed_image->pixels + (ctx->dest_width * batch->first_row);
    dest_end_p = dest_p + (ctx->dest_width * batch->n_rows);

    for ( ; dest_p < dest_end_p; src_p++, dest_p++)
    {
        ChafaColor col;
        gint index;

        col = chafa_color8_fetch_from_rgba8 (src_p);
        index = quantize_pixel (ctx, &chash, col);
        *dest_p = index;
    }

    chafa_color_hash_deinit (&chash);
}

static void
draw_pixels (DrawPixelsCtx *ctx)
{
    chafa_process_batches (ctx,
                           (GFunc) draw_pixels_pass_1_worker,
                           NULL,
                           ctx->dest_height,
                           g_get_num_processors (),
                           1);

    if (chafa_palette_get_type (&ctx->indexed_image->palette) == CHAFA_PALETTE_TYPE_DYNAMIC_256)
        chafa_palette_generate (&ctx->indexed_image->palette,
                                ctx->scaled_data, ctx->dest_width * ctx->dest_height,
                                ctx->color_space);

    chafa_process_batches (ctx,
                           (GFunc) draw_pixels_pass_2_worker,
                           NULL,
                           ctx->dest_height,
                           g_get_num_processors (),
                           1);
}

ChafaIndexedImage *
chafa_indexed_image_new (gint width, gint height, const ChafaPalette *palette)
{
    ChafaIndexedImage *indexed_image;

    indexed_image = g_new0 (ChafaIndexedImage, 1);
    indexed_image->width = width;
    indexed_image->height = height;
    indexed_image->pixels = g_malloc (width * height);

    chafa_palette_copy (palette, &indexed_image->palette);
    chafa_palette_set_transparent_index (&indexed_image->palette, 255);

    return indexed_image;
}

void
chafa_indexed_image_destroy (ChafaIndexedImage *indexed_image)
{
    g_free (indexed_image->pixels);
    g_free (indexed_image);
}

void
chafa_indexed_image_draw_pixels (ChafaIndexedImage *indexed_image,
                                 ChafaColorSpace color_space,
                                 ChafaPixelType src_pixel_type,
                                 gconstpointer src_pixels,
                                 gint src_width, gint src_height, gint src_rowstride,
                                 gint dest_width, gint dest_height)
{
    DrawPixelsCtx ctx;

    g_return_if_fail (dest_width == indexed_image->width);
    g_return_if_fail (dest_height <= indexed_image->height);

    dest_width = MIN (dest_width, indexed_image->width);
    dest_height = MIN (dest_height, indexed_image->height);

    ctx.indexed_image = indexed_image;
    ctx.color_space = color_space;
    ctx.src_pixel_type = src_pixel_type;
    ctx.src_pixels = src_pixels;
    ctx.src_width = src_width;
    ctx.src_height = src_height;
    ctx.src_rowstride = src_rowstride;
    ctx.dest_width = dest_width;
    ctx.dest_height = dest_height;

    ctx.scaled_data = g_new (guint32, dest_width * dest_height);
    ctx.scale_ctx = smol_scale_new ((SmolPixelType) src_pixel_type,
                                    (const guint32 *) src_pixels,
                                    src_width,
                                    src_height,
                                    src_rowstride,
                                    SMOL_PIXEL_RGBA8_PREMULTIPLIED,
                                    NULL,
                                    dest_width,
                                    dest_height,
                                    dest_width * sizeof (guint32));

    draw_pixels (&ctx);

    memset (indexed_image->pixels + indexed_image->width * dest_height,
            0,
            indexed_image->width * (indexed_image->height - dest_height));

    smol_scale_destroy (ctx.scale_ctx);
    g_free (ctx.scaled_data);
}
