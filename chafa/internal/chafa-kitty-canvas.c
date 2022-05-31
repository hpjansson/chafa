/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2019-2022 Hans Petter Jansson
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
#include "internal/chafa-kitty-canvas.h"
#include "internal/chafa-string-util.h"

typedef struct
{
    ChafaKittyCanvas *kitty_canvas;
    GString *out_str;
}
BuildCtx;

typedef struct
{
    ChafaKittyCanvas *kitty_canvas;
    SmolScaleCtx *scale_ctx;
}
DrawCtx;

ChafaKittyCanvas *
chafa_kitty_canvas_new (gint width, gint height)
{
    ChafaKittyCanvas *kitty_canvas;

    kitty_canvas = g_new (ChafaKittyCanvas, 1);
    kitty_canvas->width = width;
    kitty_canvas->height = height;
    kitty_canvas->rgba_image = g_malloc (width * height * sizeof (guint32));

    return kitty_canvas;
}

void
chafa_kitty_canvas_destroy (ChafaKittyCanvas *kitty_canvas)
{
    g_free (kitty_canvas->rgba_image);
    g_free (kitty_canvas);
}

static void
draw_pixels_worker (ChafaBatchInfo *batch, const DrawCtx *ctx)
{
    smol_scale_batch_full (ctx->scale_ctx,
                           ((guint32 *) ctx->kitty_canvas->rgba_image) + (ctx->kitty_canvas->width * batch->first_row),
                           batch->first_row,
                           batch->n_rows);
}

void
chafa_kitty_canvas_draw_all_pixels (ChafaKittyCanvas *kitty_canvas, ChafaPixelType src_pixel_type,
                                    gconstpointer src_pixels,
                                    gint src_width, gint src_height, gint src_rowstride)
{
    DrawCtx ctx;

    g_return_if_fail (kitty_canvas != NULL);
    g_return_if_fail (src_pixel_type < CHAFA_PIXEL_MAX);
    g_return_if_fail (src_pixels != NULL);
    g_return_if_fail (src_width >= 0);
    g_return_if_fail (src_height >= 0);

    if (src_width == 0 || src_height == 0)
        return;

    ctx.kitty_canvas = kitty_canvas;
    ctx.scale_ctx = smol_scale_new_full ((SmolPixelType) src_pixel_type,
                                         (const guint32 *) src_pixels,
                                         src_width,
                                         src_height,
                                         src_rowstride,
                                         SMOL_PIXEL_RGBA8_PREMULTIPLIED,
                                         NULL,
                                         kitty_canvas->width,
                                         kitty_canvas->height,
                                         kitty_canvas->width * sizeof (guint32),
                                         NULL,
                                         &ctx);

    chafa_process_batches (&ctx,
                           (GFunc) draw_pixels_worker,
                           NULL,
                           kitty_canvas->height,
                           chafa_get_n_actual_threads (),
                           1);

    smol_scale_destroy (ctx.scale_ctx);
}

static void
encode_chunk (GString *gs, const guint8 *start, const guint8 *end)
{
    ChafaBase64 base64;

    chafa_base64_init (&base64);
    chafa_base64_encode (&base64, gs, start, end - start);
    chafa_base64_encode_end (&base64, gs);
    chafa_base64_deinit (&base64);
}

void
chafa_kitty_canvas_build_ansi (ChafaKittyCanvas *kitty_canvas, ChafaTermInfo *term_info, GString *out_str,
                               gint width_cells, gint height_cells)
{
    const guint8 *p, *last;
    gchar seq [CHAFA_TERM_SEQ_LENGTH_MAX + 1];

    *chafa_term_info_emit_begin_kitty_immediate_image_v1 (term_info, seq,
                                                          32,
                                                          kitty_canvas->width,
                                                          kitty_canvas->height,
                                                          width_cells,
                                                          height_cells) = '\0';
    g_string_append (out_str, seq);

    last = kitty_canvas->rgba_image + kitty_canvas->width * kitty_canvas->height * sizeof (guint32);

    for (p = kitty_canvas->rgba_image; p < last; )
    {
        const guint8 *end;

        end = p + 512;
        if (end > last)
            end = last;

        *chafa_term_info_emit_begin_kitty_image_chunk (term_info, seq) = '\0';
        g_string_append (out_str, seq);

        encode_chunk (out_str, p, end);

        *chafa_term_info_emit_end_kitty_image_chunk (term_info, seq) = '\0';
        g_string_append (out_str, seq);

        p = end;
    }

    *chafa_term_info_emit_end_kitty_image (term_info, seq) = '\0';
    g_string_append (out_str, seq);
}
