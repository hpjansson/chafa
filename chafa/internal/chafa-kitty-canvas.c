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
#include "internal/chafa-pixops.h"
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
    ChafaColor bg_color;
    gboolean flatten_alpha;
}
DrawCtx;

/* Kitty's cell-based placeholders use Unicode diacritics to encode each
 * cell's row/col offsets. The below table maps integers to code points
 * using this scheme. */

#define ROWCOLUMN_UNICHAR 0x10eeeeU
#define ENCODING_DIACRITIC_MAX 297

static const guint32 encoding_diacritics [ENCODING_DIACRITIC_MAX] =
{
    0x0305,  0x030d,  0x030e,  0x0310,  0x0312,  0x033d,  0x033e,  0x033f,
    0x0346,  0x034a,  0x034b,  0x034c,  0x0350,  0x0351,  0x0352,  0x0357,
    0x035b,  0x0363,  0x0364,  0x0365,  0x0366,  0x0367,  0x0368,  0x0369,
    0x036a,  0x036b,  0x036c,  0x036d,  0x036e,  0x036f,  0x0483,  0x0484,
    0x0485,  0x0486,  0x0487,  0x0592,  0x0593,  0x0594,  0x0595,  0x0597,
    0x0598,  0x0599,  0x059c,  0x059d,  0x059e,  0x059f,  0x05a0,  0x05a1,
    0x05a8,  0x05a9,  0x05ab,  0x05ac,  0x05af,  0x05c4,  0x0610,  0x0611,
    0x0612,  0x0613,  0x0614,  0x0615,  0x0616,  0x0617,  0x0657,  0x0658,
    0x0659,  0x065a,  0x065b,  0x065d,  0x065e,  0x06d6,  0x06d7,  0x06d8,
    0x06d9,  0x06da,  0x06db,  0x06dc,  0x06df,  0x06e0,  0x06e1,  0x06e2,
    0x06e4,  0x06e7,  0x06e8,  0x06eb,  0x06ec,  0x0730,  0x0732,  0x0733,
    0x0735,  0x0736,  0x073a,  0x073d,  0x073f,  0x0740,  0x0741,  0x0743,
    0x0745,  0x0747,  0x0749,  0x074a,  0x07eb,  0x07ec,  0x07ed,  0x07ee,
    0x07ef,  0x07f0,  0x07f1,  0x07f3,  0x0816,  0x0817,  0x0818,  0x0819,
    0x081b,  0x081c,  0x081d,  0x081e,  0x081f,  0x0820,  0x0821,  0x0822,
    0x0823,  0x0825,  0x0826,  0x0827,  0x0829,  0x082a,  0x082b,  0x082c,

    /* 128 */

    0x082d,  0x0951,  0x0953,  0x0954,  0x0f82,  0x0f83,  0x0f86,  0x0f87,
    0x135d,  0x135e,  0x135f,  0x17dd,  0x193a,  0x1a17,  0x1a75,  0x1a76,
    0x1a77,  0x1a78,  0x1a79,  0x1a7a,  0x1a7b,  0x1a7c,  0x1b6b,  0x1b6d,
    0x1b6e,  0x1b6f,  0x1b70,  0x1b71,  0x1b72,  0x1b73,  0x1cd0,  0x1cd1,
    0x1cd2,  0x1cda,  0x1cdb,  0x1ce0,  0x1dc0,  0x1dc1,  0x1dc3,  0x1dc4,
    0x1dc5,  0x1dc6,  0x1dc7,  0x1dc8,  0x1dc9,  0x1dcb,  0x1dcc,  0x1dd1,
    0x1dd2,  0x1dd3,  0x1dd4,  0x1dd5,  0x1dd6,  0x1dd7,  0x1dd8,  0x1dd9,
    0x1dda,  0x1ddb,  0x1ddc,  0x1ddd,  0x1dde,  0x1ddf,  0x1de0,  0x1de1,
    0x1de2,  0x1de3,  0x1de4,  0x1de5,  0x1de6,  0x1dfe,  0x20d0,  0x20d1,
    0x20d4,  0x20d5,  0x20d6,  0x20d7,  0x20db,  0x20dc,  0x20e1,  0x20e7,
    0x20e9,  0x20f0,  0x2cef,  0x2cf0,  0x2cf1,  0x2de0,  0x2de1,  0x2de2,
    0x2de3,  0x2de4,  0x2de5,  0x2de6,  0x2de7,  0x2de8,  0x2de9,  0x2dea,
    0x2deb,  0x2dec,  0x2ded,  0x2dee,  0x2def,  0x2df0,  0x2df1,  0x2df2,
    0x2df3,  0x2df4,  0x2df5,  0x2df6,  0x2df7,  0x2df8,  0x2df9,  0x2dfa,
    0x2dfb,  0x2dfc,  0x2dfd,  0x2dfe,  0x2dff,  0xa66f,  0xa67c,  0xa67d,
    0xa6f0,  0xa6f1,  0xa8e0,  0xa8e1,  0xa8e2,  0xa8e3,  0xa8e4,  0xa8e5,

    /* 256 */

    0xa8e6,  0xa8e7,  0xa8e8,  0xa8e9,  0xa8ea,  0xa8eb,  0xa8ec,  0xa8ed,
    0xa8ee,  0xa8ef,  0xa8f0,  0xa8f1,  0xaab0,  0xaab2,  0xaab3,  0xaab7,
    0xaab8,  0xaabe,  0xaabf,  0xaac1,  0xfe20,  0xfe21,  0xfe22,  0xfe23,
    0xfe24,  0xfe25,  0xfe26,  0x10a0f, 0x10a38, 0x1d185, 0x1d186, 0x1d187,
    0x1d188, 0x1d189, 0x1d1aa, 0x1d1ab, 0x1d1ac, 0x1d1ad, 0x1d242, 0x1d243,
    0x1d244

    /* 297 */
};

ChafaKittyCanvas *
chafa_kitty_canvas_new (gint width, gint height)
{
    ChafaKittyCanvas *kitty_canvas;

    kitty_canvas = g_new0 (ChafaKittyCanvas, 1);
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

    /* FIXME: Smolscale should be able to do this */
    if (ctx->flatten_alpha)
        chafa_composite_rgba_on_solid_color (ctx->bg_color,
                                             ctx->kitty_canvas->rgba_image,
                                             ctx->kitty_canvas->width,
                                             batch->first_row,
                                             batch->n_rows);
}

void
chafa_kitty_canvas_draw_all_pixels (ChafaKittyCanvas *kitty_canvas, ChafaPixelType src_pixel_type,
                                    gconstpointer src_pixels,
                                    gint src_width, gint src_height, gint src_rowstride,
                                    ChafaColor bg_color)
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
    ctx.bg_color = bg_color;
    ctx.flatten_alpha = bg_color.ch [3] == 0;

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

static void
escape_string (const gchar *in, gchar *out)
{
    gint i, j;

    for (i = 0, j = 0; in [i]; i++)
    {
        out [j++] = in [i];
        if (in [i] == 0x1b)
            out [j++] = 0x1b;
    }

    out [j] = '\0';
}

static void
append_escaped (GString *gs, const gchar *in, ChafaPassthrough passthrough)
{
    gchar escaped_seq [CHAFA_TERM_SEQ_LENGTH_MAX * 2 + 1];

    if (passthrough == CHAFA_PASSTHROUGH_TMUX)
    {
        escape_string (in, escaped_seq);
        g_string_append (gs, escaped_seq);
    }
    else
    {
        g_string_append (gs, in);
    }
}

static void
build_begin_passthrough (ChafaTermInfo *term_info, GString *out_str,
                         ChafaPassthrough passthrough)
{
    gchar seq [CHAFA_TERM_SEQ_LENGTH_MAX + 1];

    if (passthrough == CHAFA_PASSTHROUGH_TMUX)
    {
        *chafa_term_info_emit_begin_tmux_passthrough (term_info, seq) = '\0';
        g_string_append (out_str, seq);
    }
}

static void
build_end_passthrough (ChafaTermInfo *term_info, GString *out_str,
                       ChafaPassthrough passthrough)
{
    gchar seq [CHAFA_TERM_SEQ_LENGTH_MAX + 1];

    if (passthrough == CHAFA_PASSTHROUGH_TMUX)
    {
        *chafa_term_info_emit_end_tmux_passthrough (term_info, seq) = '\0';
        g_string_append (out_str, seq);
    }
}

static void
build_image_chunks (ChafaKittyCanvas *kitty_canvas, ChafaTermInfo *term_info, GString *out_str,
                    ChafaPassthrough passthrough)
{
    const guint8 *p, *last;
    gchar seq [CHAFA_TERM_SEQ_LENGTH_MAX + 1];

    last = ((guint8 *) kitty_canvas->rgba_image)
        + kitty_canvas->width * kitty_canvas->height * sizeof (guint32);

    for (p = kitty_canvas->rgba_image; p < last; )
    {
        const guint8 *end;

        end = p + 512;
        if (end > last)
            end = last;

        build_begin_passthrough (term_info, out_str, passthrough);
        *chafa_term_info_emit_begin_kitty_image_chunk (term_info, seq) = '\0';
        append_escaped (out_str, seq, passthrough);

        encode_chunk (out_str, p, end);

        *chafa_term_info_emit_end_kitty_image_chunk (term_info, seq) = '\0';
        append_escaped (out_str, seq, passthrough);
        build_end_passthrough (term_info, out_str, passthrough);

        p = end;
    }

    build_begin_passthrough (term_info, out_str, passthrough);
    *chafa_term_info_emit_end_kitty_image (term_info, seq) = '\0';
    append_escaped (out_str, seq, passthrough);
    build_end_passthrough (term_info, out_str, passthrough);
}

static void
build_immediate (ChafaKittyCanvas *kitty_canvas, ChafaTermInfo *term_info, GString *out_str,
                 gint width_cells, gint height_cells)
{
    gchar seq [CHAFA_TERM_SEQ_LENGTH_MAX + 1];

    *chafa_term_info_emit_begin_kitty_immediate_image_v1 (term_info, seq,
                                                          32,
                                                          kitty_canvas->width,
                                                          kitty_canvas->height,
                                                          width_cells,
                                                          height_cells) = '\0';
    g_string_append (out_str, seq);

    build_image_chunks (kitty_canvas, term_info, out_str, CHAFA_PASSTHROUGH_NONE);
}

static void
build_unicode_placement (ChafaTermInfo *term_info,
                         GString *out_str,
                         gint width_cells, gint height_cells,
                         gint placement_id)
{
    gchar seq [CHAFA_TERM_SEQ_LENGTH_MAX * 2 + 1];
    gchar *p0;
    gchar *row;
    gint row_ofs;
    gint i, j;

    g_assert (placement_id >= 1);
    g_assert (placement_id <= 255);

    width_cells = MIN (width_cells, ENCODING_DIACRITIC_MAX - 1);
    height_cells = MIN (height_cells, ENCODING_DIACRITIC_MAX - 1);

    row = g_malloc (width_cells * (6 * 3) + 1);

    for (i = 0; i < height_cells; i++)
    {
        /* Encode the image ID in the foreground color */

        p0 = chafa_term_info_emit_set_color_fg_256 (term_info, seq, placement_id);
        g_string_append_len (out_str, seq, p0 - seq);

        /* Reposition after previous row */

        if (i > 0)
        {
            p0 = chafa_term_info_emit_cursor_left (term_info, seq, width_cells);
            p0 = chafa_term_info_emit_cursor_down_scroll (term_info, p0);
            g_string_append_len (out_str, seq, p0 - seq);
        }

        /* Print the row */

        row_ofs = 0;

        for (j = 0; j < width_cells; j++)
        {
            row_ofs += g_unichar_to_utf8 (ROWCOLUMN_UNICHAR, row + row_ofs);
            row_ofs += g_unichar_to_utf8 (encoding_diacritics [i], row + row_ofs);
            row_ofs += g_unichar_to_utf8 (encoding_diacritics [j], row + row_ofs);
        }

        g_string_append_len (out_str, row, row_ofs);
    }

    /* Reset foreground color */

    p0 = chafa_term_info_emit_reset_color_fg (term_info, seq);
    g_string_append_len (out_str, seq, p0 - seq);

    g_free (row);
}

static void
build_unicode_virtual (ChafaKittyCanvas *kitty_canvas, ChafaTermInfo *term_info, GString *out_str,
                       gint width_cells, gint height_cells, gint placement_id,
                       ChafaPassthrough passthrough)
{
    gchar seq [CHAFA_TERM_SEQ_LENGTH_MAX + 1];

    build_begin_passthrough (term_info, out_str, passthrough);
    *chafa_term_info_emit_begin_kitty_immediate_virt_image_v1 (term_info, seq,
                                                               32,
                                                               kitty_canvas->width,
                                                               kitty_canvas->height,
                                                               width_cells,
                                                               height_cells,
                                                               placement_id) = '\0';
    append_escaped (out_str, seq, passthrough);
    build_end_passthrough (term_info, out_str, passthrough);

    build_image_chunks (kitty_canvas, term_info, out_str, passthrough);

    build_unicode_placement (term_info, out_str, width_cells, height_cells,
                             placement_id);
}

void
chafa_kitty_canvas_build_ansi (ChafaKittyCanvas *kitty_canvas,
                               ChafaTermInfo *term_info, GString *out_str,
                               gint width_cells, gint height_cells,
                               gint placement_id,
                               ChafaPassthrough passthrough)
{
    if (passthrough == CHAFA_PASSTHROUGH_NONE)
    {
        build_immediate (kitty_canvas, term_info, out_str,
                         width_cells, height_cells);
    }
    else
    {
        /* Make IDs in the first <256 range predictable, but as the range
         * cycles we add one to skip over every ID==0 */
        if (placement_id > 255)
            placement_id = 1 + (placement_id % 255);

        build_unicode_virtual (kitty_canvas, term_info, out_str,
                               width_cells, height_cells,
                               placement_id, passthrough);
    }
}
