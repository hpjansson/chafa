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
#include "internal/chafa-batch.h"
#include "internal/chafa-bitfield.h"
#include "internal/chafa-indexed-image.h"
#include "internal/chafa-sixel-canvas.h"
#include "internal/chafa-string-util.h"

#define SIXEL_CELL_HEIGHT 6

typedef struct
{
    ChafaSixelCanvas *sixel_canvas;
    GString *out_str;
}
BuildSixelsCtx;

typedef struct
{
    /* Lower six bytes are vertical pixel strip; LSB is bottom pixel */
    guint64 d;
}
SixelData;

typedef struct
{
    SixelData *data;
    ChafaBitfield filter_bits;
}
SixelRow;

static gint
round_up_to_multiple_of (gint value, gint m)
{
    value = value + m - 1;
    return value - (value % m);
}

ChafaSixelCanvas *
chafa_sixel_canvas_new (gint width, gint height,
                        ChafaColorSpace color_space,
                        const ChafaPalette *palette,
                        const ChafaDither *dither)
{
    ChafaSixelCanvas *sixel_canvas;

    sixel_canvas = g_new (ChafaSixelCanvas, 1);
    sixel_canvas->width = width;
    sixel_canvas->height = height;
    sixel_canvas->color_space = color_space;
    sixel_canvas->image = chafa_indexed_image_new (width, round_up_to_multiple_of (height, SIXEL_CELL_HEIGHT),
                                                   palette, dither);

    return sixel_canvas;
}

void
chafa_sixel_canvas_destroy (ChafaSixelCanvas *sixel_canvas)
{
    chafa_indexed_image_destroy (sixel_canvas->image);
    g_free (sixel_canvas);
}

void
chafa_sixel_canvas_draw_all_pixels (ChafaSixelCanvas *sixel_canvas, ChafaPixelType src_pixel_type,
                                    gconstpointer src_pixels,
                                    gint src_width, gint src_height, gint src_rowstride)
{
    g_return_if_fail (sixel_canvas != NULL);
    g_return_if_fail (src_pixel_type < CHAFA_PIXEL_MAX);
    g_return_if_fail (src_pixels != NULL);
    g_return_if_fail (src_width >= 0);
    g_return_if_fail (src_height >= 0);

    if (src_width == 0 || src_height == 0)
        return;

    chafa_indexed_image_draw_pixels (sixel_canvas->image,
                                     sixel_canvas->color_space,
                                     src_pixel_type,
                                     src_pixels,
                                     src_width, src_height, src_rowstride,
                                     sixel_canvas->width, sixel_canvas->height);
}

#define FILTER_BANK_WIDTH 64

static void
filter_set (SixelRow *srow, guint8 pen, gint bank)
{
    chafa_bitfield_set_bit (&srow->filter_bits, bank * 256 + (gint) pen, TRUE);
}

static gboolean
filter_get (const SixelRow *srow, guint8 pen, gint bank)
{
    return chafa_bitfield_get_bit (&srow->filter_bits, bank * 256 + (gint) pen);
}

static void
fetch_sixel_row (SixelRow *srow, const guint8 *pixels, gint width)
{
    const guint8 *pixels_end, *p;
    SixelData *sdata = srow->data;
    gint x;

    /* The ordering of output bytes is 351240; this is the inverse of
     * 140325. see sixel_data_do_schar(). */

    for (pixels_end = pixels + width, x = 0; pixels < pixels_end; pixels++, x++)
    {
        guint64 d;
        gint bank = x / FILTER_BANK_WIDTH;

        p = pixels;

        filter_set (srow, *p, bank);
        d = (guint64) *p;
        p += width;

        filter_set (srow, *p, bank);
        d |= (guint64) *p << (3 * 8);
        p += width;

        filter_set (srow, *p, bank);
        d |= (guint64) *p << (2 * 8);
        p += width;

        filter_set (srow, *p, bank);
        d |= (guint64) *p << (5 * 8);
        p += width;

        filter_set (srow, *p, bank);
        d |= (guint64) *p << (1 * 8);
        p += width;

        filter_set (srow, *p, bank);
        d |= (guint64) *p << (4 * 8);

        (sdata++)->d = d;
    }
} 

static gchar
sixel_data_to_schar (const SixelData *sdata, guint64 expanded_pen)
{
    guint64 a;
    gchar c;

    a = ~(sdata->d ^ expanded_pen);

    /* Matching bytes will now contain 0xff. Any other value is a mismatch. */

    a &= (a & 0x0000f0f0f0f0f0f0) >> 4;
    a &= (a & 0x00000c0c0c0c0c0c) >> 2;
    a &= (a & 0x0000020202020202) >> 1;

    /* Matching bytes will now contain 0x01. Misses contain 0x00. */

    a |= a >> (24 - 1);
    a |= a >> (16 - 2);
    a |= a >> (8 - 4);

    /* Set bits are now packed in the lower 6 bits, reordered like this:
     *
     * 012345 -> 03/14/25 -> 14/0325 -> 140325 */

    c = a & 0x3f;

    return '?' + c;
}

static gchar *
format_schar_reps (gchar rep_schar, gint n_reps, gchar *p)
{
    g_assert (n_reps > 0);

    for (;;)
    {
        if (n_reps < 4)
        {
            do *(p++) = rep_schar;
            while (--n_reps);

            goto out;
        }
        else if (n_reps < 255)
        {
            *(p++) = '!';
            p = chafa_format_dec_u8 (p, n_reps);
            *(p++) = rep_schar;
            goto out;
        }
        else
        {
            strcpy (p, "!255");
            p += 4;
            *(p++) = rep_schar;
            n_reps -= 255;

            if (n_reps == 0)
                goto out;
        }
    }

out:
    return p;
}

static gchar *
format_pen (guint8 pen, gchar *p)
{
    *(p++) = '#';
    return chafa_format_dec_u8 (p, pen);
}

/* force_full_width is a workaround for a bug in mlterm; we need to
 * draw the entire first row even if the rightmost pixels are transparent,
 * otherwise the first row with non-transparent pixels will have
 * garbage rendered in it */
static gchar *
build_sixel_row_ansi (const ChafaSixelCanvas *scanvas, const SixelRow *srow, gchar *p, gboolean force_full_width)
{
    gint pen = 0;
    gboolean need_cr = FALSE;
    gboolean need_cr_next = FALSE;
    const SixelData *sdata = srow->data;
    gint width = scanvas->width;

    do
    {
        guint64 expanded_pen;
        gboolean need_pen = TRUE;
        gchar rep_schar;
        gint n_reps;
        gint i;

        if (pen == chafa_palette_get_transparent_index (&scanvas->image->palette))
            continue;

        /* Assign pen value to each of lower six bytes */
        expanded_pen = pen;
        expanded_pen |= expanded_pen << 8;
        expanded_pen |= expanded_pen << 16;
        expanded_pen |= expanded_pen << 16;

        rep_schar = 0;
        n_reps = 0;

        for (i = 0; i < width; )
        {
            gint step = MIN (FILTER_BANK_WIDTH, width - i);
            gchar schar;

            /* Skip over FILTER_BANK_WIDTH sixels at once if possible */

            if (!filter_get (srow, pen, i / FILTER_BANK_WIDTH))
            {
                if (rep_schar != '?' && rep_schar != 0)
                {
                    if (need_cr)
                    {
                        *(p++) = '$';
                        need_cr = FALSE;
                    }
                    if (need_pen)
                    {
                        p = format_pen (pen, p);
                        need_pen = FALSE;
                    }

                    p = format_schar_reps (rep_schar, n_reps, p);
                    need_cr_next = TRUE;
                    n_reps = 0;
                }

                rep_schar = '?';
                n_reps += step;
                i += step;
                continue;
            }

            /* The pen appears in this bank; iterate over sixels */

            for ( ; step > 0; step--, i++)
            {
                schar = sixel_data_to_schar (&sdata [i], expanded_pen);

                if (schar == rep_schar)
                {
                    n_reps++;
                }
                else if (rep_schar == 0)
                {
                    rep_schar = schar;
                    n_reps = 1;
                }
                else
                {
                    if (need_cr)
                    {
                        *(p++) = '$';
                        need_cr = FALSE;
                    }
                    if (need_pen)
                    {
                        p = format_pen (pen, p);
                        need_pen = FALSE;
                    }

                    p = format_schar_reps (rep_schar, n_reps, p);
                    need_cr_next = TRUE;

                    rep_schar = schar;
                    n_reps = 1;
                }
            }
        }

        if (rep_schar != '?' || force_full_width)
        {
            if (need_cr)
            {
                *(p++) = '$';
                need_cr = FALSE;
            }
            if (need_pen)
            {
                p = format_pen (pen, p);
                need_pen = FALSE;
            }

            p = format_schar_reps (rep_schar, n_reps, p);
            need_cr_next = TRUE;

            /* Only need to do this for a single pen */
            force_full_width = FALSE;
        }

        need_cr = need_cr_next;
    }
    while (++pen < chafa_palette_get_n_colors (&scanvas->image->palette));

    *(p++) = '-';
    return p;
}

static void
build_sixel_row_worker (ChafaBatchInfo *batch, const BuildSixelsCtx *ctx)
{
    SixelRow srow;
    gchar *sixel_ansi, *p;
    gint n_sixel_rows;
    gint i;

    n_sixel_rows = (batch->n_rows + SIXEL_CELL_HEIGHT - 1) / SIXEL_CELL_HEIGHT;
    srow.data = g_malloc (sizeof (SixelData) * ctx->sixel_canvas->width);
    chafa_bitfield_init (&srow.filter_bits, ((ctx->sixel_canvas->width + FILTER_BANK_WIDTH - 1) / FILTER_BANK_WIDTH) * 256);

    sixel_ansi = p = g_malloc (256 * (ctx->sixel_canvas->width + 5) * n_sixel_rows + 1);

    for (i = 0; i < n_sixel_rows; i++)
    {
        fetch_sixel_row (&srow,
                         ctx->sixel_canvas->image->pixels
                         + ctx->sixel_canvas->image->width * (batch->first_row + i * SIXEL_CELL_HEIGHT),
                         ctx->sixel_canvas->image->width);
        p = build_sixel_row_ansi (ctx->sixel_canvas, &srow, p,
                                  (i == 0) || (i == n_sixel_rows - 1)
                                  ? TRUE : FALSE);
        chafa_bitfield_clear (&srow.filter_bits);
    }

    batch->ret_p = sixel_ansi;
    batch->ret_n = p - sixel_ansi;

    chafa_bitfield_deinit (&srow.filter_bits);
    g_free (srow.data);
}

static void
build_sixel_row_post (ChafaBatchInfo *batch, BuildSixelsCtx *ctx)
{
    g_string_append_len (ctx->out_str, batch->ret_p, batch->ret_n);
    g_free (batch->ret_p);
}

static void
build_sixel_palette (ChafaSixelCanvas *sixel_canvas, GString *out_str)
{
    gchar str [256 * 20 + 1];
    gchar *p = str;
    gint first_color;
    gint pen;

    first_color = chafa_palette_get_first_color (&sixel_canvas->image->palette);

    for (pen = 0; pen < chafa_palette_get_n_colors (&sixel_canvas->image->palette); pen++)
    {
        const ChafaColor *col;

        if (pen == chafa_palette_get_transparent_index (&sixel_canvas->image->palette))
            continue;

        col = chafa_palette_get_color (&sixel_canvas->image->palette, CHAFA_COLOR_SPACE_RGB,
                                       first_color + pen);
        *(p++) = '#';
        p = chafa_format_dec_u8 (p, pen);
        *(p++) = ';';
        *(p++) = '2';  /* Color space: RGB */
        *(p++) = ';';

        /* Sixel color channel range is 0..100 */

        p = chafa_format_dec_u8 (p, (col->ch [0] * 100) / 255);
        *(p++) = ';';
        p = chafa_format_dec_u8 (p, (col->ch [1] * 100) / 255);
        *(p++) = ';';
        p = chafa_format_dec_u8 (p, (col->ch [2] * 100) / 255);
    }

    g_string_append_len (out_str, str, p - str);
}

void
chafa_sixel_canvas_build_ansi (ChafaSixelCanvas *sixel_canvas, GString *out_str)
{
    BuildSixelsCtx ctx;

    g_assert (sixel_canvas->image->height % SIXEL_CELL_HEIGHT == 0);

    ctx.sixel_canvas = sixel_canvas;
    ctx.out_str = out_str;

    build_sixel_palette (sixel_canvas, out_str);

    chafa_process_batches (&ctx,
                           (GFunc) build_sixel_row_worker,
                           (GFunc) build_sixel_row_post,
                           sixel_canvas->image->height,
                           chafa_get_n_actual_threads (),
                           SIXEL_CELL_HEIGHT);
}
