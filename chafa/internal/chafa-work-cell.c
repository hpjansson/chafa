/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2021 Hans Petter Jansson
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

#include <math.h>
#include <string.h>
#include <glib.h>
#include "chafa.h"
#include "internal/chafa-private.h"
#include "internal/chafa-pixops.h"
#include "internal/chafa-work-cell.h"

/* Used for cell initialization. May be added up over multiple cells, so a
 * low multiple needs to fit in an integer. */
#define SYMBOL_ERROR_MAX (G_MAXINT / 8)

/* Max candidates to consider in pick_symbol_and_colors_fast(). This is also
 * limited by a similar constant in chafa-symbol-map.c */
#define N_CANDIDATES_MAX 8

typedef struct
{
    ChafaPixel fg;
    ChafaPixel bg;
    gint error;
}
SymbolEval;

typedef struct
{
    ChafaPixel fg;
    ChafaPixel bg;
    gint error [2];
}
SymbolEval2;

static void
accum_to_color (const ChafaColorAccum *accum, ChafaColor *color)
{
    gint i;

    for (i = 0; i < 4; i++)
        color->ch [i] = accum->ch [i];
}

/* pixels_out must point to CHAFA_SYMBOL_N_PIXELS-element array */
static void
fetch_canvas_pixel_block (const ChafaPixel *src_image, gint src_width,
                          ChafaPixel *pixels_out, gint cx, gint cy)
{
    const ChafaPixel *row_p;
    const ChafaPixel *end_p;
    gint i = 0;

    row_p = src_image + cy * CHAFA_SYMBOL_HEIGHT_PIXELS * src_width + cx * CHAFA_SYMBOL_WIDTH_PIXELS;
    end_p = row_p + (src_width * CHAFA_SYMBOL_HEIGHT_PIXELS);

    for ( ; row_p < end_p; row_p += src_width)
    {
        const ChafaPixel *p0 = row_p;
        const ChafaPixel *p1 = p0 + CHAFA_SYMBOL_WIDTH_PIXELS;

        for ( ; p0 < p1; p0++)
            pixels_out [i++] = *p0;
    }
}

static void
calc_colors_plain (const ChafaPixel *block, ChafaColorAccum *accums, const guint8 *cov)
{
    const guint8 *in_u8 = (const guint8 *) block;
    gint i;

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        gint16 *out_s16 = (gint16 *) (accums + *cov++);

        *out_s16++ += *in_u8++;
        *out_s16++ += *in_u8++;
        *out_s16++ += *in_u8++;
        *out_s16++ += *in_u8++;
    }
}

void
chafa_work_cell_get_mean_colors_for_symbol (const ChafaWorkCell *wcell, const ChafaSymbol *sym,
                                            ChafaColorPair *color_pair_out)
{
    const guint8 *covp = (guint8 *) &sym->coverage [0];
    ChafaColorAccum accums [2] = { 0 };

#ifdef HAVE_MMX_INTRINSICS
    if (chafa_have_mmx ())
        calc_colors_mmx (wcell->pixels, accums, covp);
    else
#endif
        calc_colors_plain (wcell->pixels, accums, covp);

    if (sym->fg_weight > 1)
        chafa_color_accum_div_scalar (&accums [1], sym->fg_weight);

    if (sym->bg_weight > 1)
        chafa_color_accum_div_scalar (&accums [0], sym->bg_weight);

    accum_to_color (&accums [1], &color_pair_out->colors [CHAFA_COLOR_PAIR_FG]);
    accum_to_color (&accums [0], &color_pair_out->colors [CHAFA_COLOR_PAIR_BG]);
}

void
chafa_work_cell_calc_mean_color (const ChafaWorkCell *wcell, ChafaColor *color_out)
{
    ChafaColorAccum accum = { 0 };
    const ChafaPixel *block = wcell->pixels;
    gint i;

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        chafa_color_accum_add (&accum, &block->col);
        block++;
    }

    chafa_color_accum_div_scalar (&accum, CHAFA_SYMBOL_N_PIXELS);
    accum_to_color (&accum, color_out);
}

/* colors must point to an array of two elements */
guint64
chafa_work_cell_to_bitmap (const ChafaWorkCell *wcell, const ChafaColorPair *color_pair)
{
    guint64 bitmap = 0;
    const ChafaPixel *block = wcell->pixels;
    gint i;

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        gint error [2];

        bitmap <<= 1;

        /* FIXME: What to do about alpha? */
        error [0] = chafa_color_diff_fast (&block [i].col, &color_pair->colors [0]);
        error [1] = chafa_color_diff_fast (&block [i].col, &color_pair->colors [1]);

        if (error [0] > error [1])
            bitmap |= 1;
    }

    return bitmap;
}

/* Get cell's pixels sorted by a specific channel. Sorts on demand and caches
 * the results. */
static const guint8 *
work_cell_get_sorted_pixels (ChafaWorkCell *wcell, gint ch)
{
    guint8 *index;
    const guint8 index_init [CHAFA_SYMBOL_N_PIXELS] =
    {
        0,   1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
        32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
        48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63
    };

    index = &wcell->pixels_sorted_index [ch] [0];

    if (wcell->have_pixels_sorted_by_channel [ch])
        return index;

    memcpy (index, index_init, CHAFA_SYMBOL_N_PIXELS);
    chafa_sort_pixel_index_by_channel (index, wcell->pixels, CHAFA_SYMBOL_N_PIXELS, ch);

    wcell->have_pixels_sorted_by_channel [ch] = TRUE;
    return index;
}

void
chafa_work_cell_init (ChafaWorkCell *wcell, const ChafaPixel *src_image,
                      gint src_width, gint cx, gint cy)
{
    memset (wcell->have_pixels_sorted_by_channel, 0,
            sizeof (wcell->have_pixels_sorted_by_channel));
    fetch_canvas_pixel_block (src_image, src_width, wcell->pixels, cx, cy);
    wcell->dominant_channel = -1;
}

static gint
work_cell_get_dominant_channel (ChafaWorkCell *wcell)
{
    const guint8 *sorted_pixels [4];
    gint best_range;
    gint best_ch;
    gint i;

    if (wcell->dominant_channel >= 0)
        return wcell->dominant_channel;

    for (i = 0; i < 4; i++)
        sorted_pixels [i] = work_cell_get_sorted_pixels (wcell, i);

    best_range = wcell->pixels [sorted_pixels [0] [CHAFA_SYMBOL_N_PIXELS - 1]].col.ch [0]
        - wcell->pixels [sorted_pixels [0] [0]].col.ch [0];
    best_ch = 0;

    for (i = 1; i < 4; i++)
    {
        gint range = wcell->pixels [sorted_pixels [i] [CHAFA_SYMBOL_N_PIXELS - 1]].col.ch [i]
            - wcell->pixels [sorted_pixels [i] [0]].col.ch [i];

        if (range > best_range)
        {
            best_range = range;
            best_ch = i;
        }
    }

    wcell->dominant_channel = best_ch;
    return wcell->dominant_channel;
}

static void
work_cell_get_dominant_channels_for_symbol (ChafaWorkCell *wcell, const ChafaSymbol *sym,
                                            gint *bg_ch_out, gint *fg_ch_out)
{
    gint16 min [2] [4] = { { G_MAXINT16, G_MAXINT16, G_MAXINT16, G_MAXINT16 },
                           { G_MAXINT16, G_MAXINT16, G_MAXINT16, G_MAXINT16 } };
    gint16 max [2] [4] = { { G_MININT16, G_MININT16, G_MININT16, G_MININT16 },
                           { G_MININT16, G_MININT16, G_MININT16, G_MININT16 } };
    gint16 range [2] [4];
    gint ch, best_ch [2];
    const guint8 *sorted_pixels [4];
    gint i, j;

    if (sym->popcount == 0)
    {
        *bg_ch_out = work_cell_get_dominant_channel (wcell);
        *fg_ch_out = -1;
        return;
    }
    else if (sym->popcount == CHAFA_SYMBOL_N_PIXELS)
    {
        *bg_ch_out = -1;
        *fg_ch_out = work_cell_get_dominant_channel (wcell);
        return;
    }

    for (i = 0; i < 4; i++)
        sorted_pixels [i] = work_cell_get_sorted_pixels (wcell, i);

    /* Get minimums */

    for (j = 0; j < 4; j++)
    {
        gint pen_a = sym->coverage [sorted_pixels [j] [0]];
        min [pen_a] [j] = wcell->pixels [sorted_pixels [j] [0]].col.ch [j];

        for (i = 1; ; i++)
        {
            gint pen_b = sym->coverage [sorted_pixels [j] [i]];
            if (pen_b != pen_a)
            {
                min [pen_b] [j] = wcell->pixels [sorted_pixels [j] [i]].col.ch [j];
                break;
            }
        }
    }

    /* Get maximums */

    for (j = 0; j < 4; j++)
    {
        gint pen_a = sym->coverage [sorted_pixels [j] [CHAFA_SYMBOL_N_PIXELS - 1]];
        max [pen_a] [j] = wcell->pixels [sorted_pixels [j] [CHAFA_SYMBOL_N_PIXELS - 1]].col.ch [j];

        for (i = CHAFA_SYMBOL_N_PIXELS - 2; ; i--)
        {
            gint pen_b = sym->coverage [sorted_pixels [j] [i]];
            if (pen_b != pen_a)
            {
                max [pen_b] [j] = wcell->pixels [sorted_pixels [j] [i]].col.ch [j];
                break;
            }
        }
    }

    /* Find channel with the greatest range */

    for (ch = 0; ch < 4; ch++)
    {
        range [0] [ch] = max [0] [ch] - min [0] [ch];
        range [1] [ch] = max [1] [ch] - min [1] [ch];
    }

    best_ch [0] = 0;
    best_ch [1] = 0;
    for (ch = 1; ch < 4; ch++)
    {
        if (range [0] [ch] > range [0] [best_ch [0]])
            best_ch [0] = ch;
        if (range [1] [ch] > range [1] [best_ch [1]])
            best_ch [1] = ch;
    }

    *bg_ch_out = best_ch [0];
    *fg_ch_out = best_ch [1];
}

/* colors_out must point to an array of two elements */
void
chafa_work_cell_get_contrasting_color_pair (ChafaWorkCell *wcell, ChafaColorPair *color_pair_out)
{
    const guint8 *sorted_pixels;

    sorted_pixels = work_cell_get_sorted_pixels (wcell, work_cell_get_dominant_channel (wcell));

    /* Choose two colors by median cut */

    color_pair_out->colors [0] = wcell->pixels [sorted_pixels [CHAFA_SYMBOL_N_PIXELS / 4]].col;
    color_pair_out->colors [1] = wcell->pixels [sorted_pixels [(CHAFA_SYMBOL_N_PIXELS * 3) / 4]].col;
}

static const ChafaPixel *
work_cell_get_nth_sorted_pixel (ChafaWorkCell *wcell, const ChafaSymbol *sym,
                                gint channel, gint pen, gint n)
{
    const guint8 *sorted_pixels;
    gint i, j;

    pen ^= 1;
    sorted_pixels = work_cell_get_sorted_pixels (wcell, channel);

    for (i = 0, j = 0; ; i++)
    {
        j += (sym->coverage [sorted_pixels [i]] ^ pen);
        if (j > n)
            return &wcell->pixels [sorted_pixels [i]];
    }

    g_assert_not_reached ();
    return NULL;
}

void
chafa_work_cell_get_median_colors_for_symbol (ChafaWorkCell *wcell, const ChafaSymbol *sym,
                                              ChafaColorPair *color_pair_out)
{
    gint bg_ch, fg_ch;

    /* This is extremely slow and makes almost no difference */
    work_cell_get_dominant_channels_for_symbol (wcell, sym, &bg_ch, &fg_ch);

    if (bg_ch < 0)
    {
        color_pair_out->colors [0] = color_pair_out->colors [1]
            = work_cell_get_nth_sorted_pixel (wcell, sym, fg_ch, 1, sym->popcount / 2)->col;
    }
    else if (fg_ch < 0)
    {
        color_pair_out->colors [0] = color_pair_out->colors [1]
            = work_cell_get_nth_sorted_pixel (wcell, sym, bg_ch, 0,
                                              (CHAFA_SYMBOL_N_PIXELS - sym->popcount) / 2)->col;
    }
    else
    {
        color_pair_out->colors [CHAFA_COLOR_PAIR_FG]
            = work_cell_get_nth_sorted_pixel (wcell, sym, fg_ch, 1, sym->popcount / 2)->col;
        color_pair_out->colors [CHAFA_COLOR_PAIR_BG]
            = work_cell_get_nth_sorted_pixel (wcell, sym, bg_ch, 0,
                                              (CHAFA_SYMBOL_N_PIXELS - sym->popcount) / 2)->col;
    }
}
