/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2020 Hans Petter Jansson
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
#include "internal/chafa-canvas-internal.h"
#include "internal/chafa-private.h"
#include "internal/chafa-pixops.h"
#include "internal/smolscale/smolscale.h"

/**
 * SECTION:chafa-canvas
 * @title: ChafaCanvas
 * @short_description: A canvas that renders to text
 *
 * A #ChafaCanvas is a canvas that can render its contents as text strings.
 *
 * To create a new #ChafaCanvas, use chafa_canvas_new (). If you want to
 * specify any parameters, like the geometry, color space and so on, you
 * must create a #ChafaCanvasConfig first.
 *
 * You can draw an image to the canvas using chafa_canvas_draw_all_pixels ()
 * and create an ANSI text (or sixel) representation of the canvas' current
 * contents using chafa_canvas_build_ansi ().
 **/

/* Used for cell initialization. May be added up over multiple cells, so a
 * low multiple needs to fit in an integer. */
#define SYMBOL_ERROR_MAX (G_MAXINT / 8)

/* Max candidates to consider in pick_symbol_and_colors_fast(). This is also
 * limited by a similar constant in chafa-symbol-map.c */
#define N_CANDIDATES_MAX 8

/* Dithering */
#define DITHER_BASE_INTENSITY_FGBG 1.0
#define DITHER_BASE_INTENSITY_16C  0.25
#define DITHER_BASE_INTENSITY_256C 0.1

typedef struct
{
    ChafaPixel pixels [CHAFA_SYMBOL_N_PIXELS];
    guint8 pixels_sorted_index [4] [CHAFA_SYMBOL_N_PIXELS];
    guint8 have_pixels_sorted_by_channel [4];
    gint dominant_channel;
}
WorkCell;

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
fetch_canvas_pixel_block (ChafaCanvas *canvas, ChafaPixel *pixels_out, gint cx, gint cy)
{
    ChafaPixel *row_p;
    ChafaPixel *end_p;
    gint i = 0;

    row_p = &canvas->pixels [cy * CHAFA_SYMBOL_HEIGHT_PIXELS * canvas->width_pixels + cx * CHAFA_SYMBOL_WIDTH_PIXELS];
    end_p = row_p + (canvas->width_pixels * CHAFA_SYMBOL_HEIGHT_PIXELS);

    for ( ; row_p < end_p; row_p += canvas->width_pixels)
    {
        ChafaPixel *p0 = row_p;
        ChafaPixel *p1 = p0 + CHAFA_SYMBOL_WIDTH_PIXELS;

        for ( ; p0 < p1; p0++)
            pixels_out [i++] = *p0;
    }
}

static void
calc_mean_color (const ChafaPixel *block, ChafaColor *color_out)
{
    ChafaColorAccum accum = { 0 };
    gint i;

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        chafa_color_accum_add (&accum, &block->col);
        block++;
    }

    chafa_color_accum_div_scalar (&accum, CHAFA_SYMBOL_N_PIXELS);
    accum_to_color (&accum, color_out);
}

static void
sort_pixel_index_by_channel (guint8 *index, const ChafaPixel *pixels, gint n_pixels, gint ch)
{
    const gint gaps [] = { 57, 23, 10, 4, 1 };
    gint g, i, j;

    /* Since we don't care about stability and the number of elements
     * is small and known in advance, use a simple in-place shellsort.
     *
     * Due to locality and callback overhead this is probably faster
     * than qsort(), although admittedly I haven't benchmarked it.
     *
     * Another option is to use radix, but since we support multiple
     * color spaces with fixed-point reals, we could get more buckets
     * than is practical. */

    for (g = 0; ; g++)
    {
        gint gap = gaps [g];

        for (i = gap; i < n_pixels; i++)
        {
            guint8 ptemp = index [i];

            for (j = i; j >= gap && pixels [index [j - gap]].col.ch [ch]
                                  > pixels [ptemp].col.ch [ch]; j -= gap)
            {
                index [j] = index [j - gap];
            }

            index [j] = ptemp;
        }

        /* After gap == 1 the array is always left sorted */
        if (gap == 1)
            break;
    }
}

/* colors must point to an array of two elements */
static guint64
block_to_bitmap (const ChafaPixel *block, ChafaColor *colors)
{
    guint64 bitmap = 0;
    gint i;

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        gint error [2];

        bitmap <<= 1;

        /* FIXME: What to do about alpha? */
        error [0] = chafa_color_diff_fast (&block [i].col, &colors [0]);
        error [1] = chafa_color_diff_fast (&block [i].col, &colors [1]);

        if (error [0] < error [1])
            bitmap |= 1;
    }

    return bitmap;
}

/* Get cell's pixels sorted by a specific channel. Sorts on demand and caches
 * the results. */
static const guint8 *
work_cell_get_sorted_pixels (WorkCell *wcell, gint ch)
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
    sort_pixel_index_by_channel (index, wcell->pixels, CHAFA_SYMBOL_N_PIXELS, ch);

    wcell->have_pixels_sorted_by_channel [ch] = TRUE;
    return index;
}

static void
work_cell_init (WorkCell *wcell, ChafaCanvas *canvas, gint cx, gint cy)
{
    memset (wcell->have_pixels_sorted_by_channel, 0,
            sizeof (wcell->have_pixels_sorted_by_channel));
    fetch_canvas_pixel_block (canvas, wcell->pixels, cx, cy);
    wcell->dominant_channel = -1;
}

static gint
work_cell_get_dominant_channel (WorkCell *wcell)
{
    const guint8 *sorted_pixels [4];
    gint best_range;
    gint best_ch;
    gint i;

    if (wcell->dominant_channel >= 0)
        return wcell->dominant_channel;

    for (i = 0; i < 4; i++)
        sorted_pixels [i] = work_cell_get_sorted_pixels (wcell, i);

    best_range = wcell->pixels [sorted_pixels [0] [63]].col.ch [0]
        - wcell->pixels [sorted_pixels [0] [0]].col.ch [0];
    best_ch = 0;

    for (i = 1; i < 4; i++)
    {
        gint range = wcell->pixels [sorted_pixels [i] [63]].col.ch [i]
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
work_cell_get_dominant_channels_for_symbol (WorkCell *wcell, const ChafaSymbol *sym,
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
    else if (sym->popcount == 64)
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
        gint pen_a = sym->coverage [sorted_pixels [j] [63]];
        max [pen_a] [j] = wcell->pixels [sorted_pixels [j] [63]].col.ch [j];

        for (i = 62; ; i--)
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
static void
work_cell_get_contrasting_color_pair (WorkCell *wcell, ChafaColor *colors_out)
{
    const guint8 *sorted_pixels;

    sorted_pixels = work_cell_get_sorted_pixels (wcell, work_cell_get_dominant_channel (wcell));

    /* Choose two colors by median cut */

    colors_out [0] = wcell->pixels [sorted_pixels [CHAFA_SYMBOL_N_PIXELS / 4]].col;
    colors_out [1] = wcell->pixels [sorted_pixels [(CHAFA_SYMBOL_N_PIXELS * 3) / 4]].col;
}

static const ChafaPixel *
work_cell_get_nth_sorted_pixel (WorkCell *wcell, const ChafaSymbol *sym,
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

static void
work_cell_get_median_pixels_for_symbol (WorkCell *wcell, const ChafaSymbol *sym,
                                        ChafaPixel *pixels_out)
{
    gint bg_ch, fg_ch;

    /* This is extremely slow and makes almost no difference */
    work_cell_get_dominant_channels_for_symbol (wcell, sym, &bg_ch, &fg_ch);

    if (bg_ch < 0)
    {
        pixels_out [0] = pixels_out [1]
            = *work_cell_get_nth_sorted_pixel (wcell, sym, fg_ch, 1, sym->popcount / 2);
    }
    else if (fg_ch < 0)
    {
        pixels_out [0] = pixels_out [1]
            = *work_cell_get_nth_sorted_pixel (wcell, sym, bg_ch, 0,
                                               (CHAFA_SYMBOL_N_PIXELS - sym->popcount) / 2);
    }
    else
    {
        pixels_out [0] = *work_cell_get_nth_sorted_pixel (wcell, sym, bg_ch, 0,
                                                          (CHAFA_SYMBOL_N_PIXELS - sym->popcount) / 2);
        pixels_out [1] = *work_cell_get_nth_sorted_pixel (wcell, sym, fg_ch, 1, sym->popcount / 2);
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

static void
eval_symbol_colors_mean (WorkCell *wcell, const ChafaSymbol *sym, SymbolEval *eval)
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

    accum_to_color (&accums [1], &eval->fg.col);
    accum_to_color (&accums [0], &eval->bg.col);
}

static void
eval_symbol_colors (ChafaCanvas *canvas, WorkCell *wcell, const ChafaSymbol *sym, SymbolEval *eval)
{
    ChafaPixel pixels [2];

    if (canvas->config.color_extractor == CHAFA_COLOR_EXTRACTOR_AVERAGE)
    {
        eval_symbol_colors_mean (wcell, sym, eval);
    }
    else
    {
        work_cell_get_median_pixels_for_symbol (wcell, sym, pixels);
        eval->bg.col = pixels [0].col;
        eval->fg.col = pixels [1].col;
    }
}

static void
eval_symbol_colors_wide (ChafaCanvas *canvas, WorkCell *wcell_a, WorkCell *wcell_b,
                         const ChafaSymbol *sym_a, const ChafaSymbol *sym_b,
                         SymbolEval2 *eval)
{
    SymbolEval part_eval [2];

    eval_symbol_colors (canvas, wcell_a, sym_a, &part_eval [0]);
    eval_symbol_colors (canvas, wcell_b, sym_b, &part_eval [1]);

    eval->fg.col = chafa_color_average_2 (part_eval [0].fg.col, part_eval [1].fg.col);
    eval->bg.col = chafa_color_average_2 (part_eval [0].bg.col, part_eval [1].bg.col);
}

static gint
calc_error_plain (const ChafaPixel *block, const ChafaColor *cols, const guint8 *cov)
{
    gint error = 0;
    gint i;

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        guint8 p = *cov++;
        const ChafaPixel *p0 = block++;

        error += chafa_color_diff_fast (&cols [p], &p0->col);
    }

    return error;
}

static gint
calc_error_with_alpha (const ChafaPixel *block, const ChafaColor *cols, const guint8 *cov, ChafaColorSpace cs)
{
    gint error = 0;
    gint i;

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        guint8 p = *cov++;
        const ChafaPixel *p0 = block++;

        error += chafa_color_diff_slow (&cols [p], &p0->col, cs);
    }

    return error;
}

static void
eval_symbol_error (ChafaCanvas *canvas, const WorkCell *wcell,
                   const ChafaSymbol *sym, SymbolEval *eval)
{
    const guint8 *covp = (guint8 *) &sym->coverage [0];
    ChafaColor cols [2] = { 0 };
    gint error;

    cols [0] = eval->bg.col;
    cols [1] = eval->fg.col;

    if (canvas->have_alpha)
    {
        error = calc_error_with_alpha (wcell->pixels, cols, covp, canvas->config.color_space);
    }
    else
    {
#ifdef HAVE_SSE41_INTRINSICS
        if (chafa_have_sse41 ())
            error = calc_error_sse41 (wcell->pixels, cols, covp);
        else
#endif
            error = calc_error_plain (wcell->pixels, cols, covp);
    }

    eval->error = error;
}

static void
eval_symbol_error_wide (ChafaCanvas *canvas, const WorkCell *wcell_a, const WorkCell *wcell_b,
                        const ChafaSymbol2 *sym, SymbolEval2 *wide_eval)
{
    SymbolEval eval [2];

    eval [0].bg = wide_eval->bg;
    eval [0].fg = wide_eval->fg;
    eval [1].bg = wide_eval->bg;
    eval [1].fg = wide_eval->fg;

    eval_symbol_error (canvas, wcell_a, &sym->sym [0], &eval [0]);
    eval_symbol_error (canvas, wcell_b, &sym->sym [1], &eval [1]);

    wide_eval->error [0] = eval [0].error;
    wide_eval->error [1] = eval [1].error;
}

static void
pick_symbol_and_colors_slow (ChafaCanvas *canvas,
                             WorkCell *wcell,
                             gunichar *sym_out,
                             ChafaColor *fg_col_out,
                             ChafaColor *bg_col_out,
                             gint *error_out)
{
    SymbolEval eval, best_eval;
    gint best_symbol = -1;
    gint i;

    best_eval.error = G_MAXINT;

    for (i = 0; canvas->config.symbol_map.symbols [i].c != 0; i++)
    {
        /* FIXME: Always evaluate space so we get fallback colors */

        if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG)
        {
            eval.fg.col = canvas->fg_color;
            eval.bg.col = canvas->bg_color;
        }
        else
        {
            eval_symbol_colors (canvas, wcell, &canvas->config.symbol_map.symbols [i], &eval);
        }

        eval_symbol_error (canvas, wcell, &canvas->config.symbol_map.symbols [i], &eval);

        if (eval.error < best_eval.error)
        {
            best_symbol = i;
            best_eval = eval;
        }
    }

#ifdef HAVE_MMX_INTRINSICS
    /* Make FPU happy again */
    if (chafa_have_mmx ())
        leave_mmx ();
#endif

    g_assert (best_symbol >= 0);

    if (error_out)
        *error_out = best_eval.error;

    *sym_out = canvas->config.symbol_map.symbols [best_symbol].c;
    *fg_col_out = best_eval.fg.col;
    *bg_col_out = best_eval.bg.col;
}

static void
pick_symbol_and_colors_wide_slow (ChafaCanvas *canvas,
                                  WorkCell *wcell_a,
                                  WorkCell *wcell_b,
                                  gunichar *sym_out,
                                  ChafaColor *fg_col_out,
                                  ChafaColor *bg_col_out,
                                  gint *error_a_out,
                                  gint *error_b_out)
{
    SymbolEval2 eval, best_eval;
    gint best_symbol = -1;
    gint i;

    best_eval.error [0] = best_eval.error [1] = SYMBOL_ERROR_MAX;

    for (i = 0; canvas->config.symbol_map.symbols2 [i].sym [0].c != 0; i++)
    {
        if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG)
        {
            eval.fg.col = canvas->fg_color;
            eval.bg.col = canvas->bg_color;
        }
        else
        {
            eval_symbol_colors_wide (canvas, wcell_a, wcell_b,
                                     &canvas->config.symbol_map.symbols2 [i].sym [0],
                                     &canvas->config.symbol_map.symbols2 [i].sym [1],
                                     &eval);
        }

        eval_symbol_error_wide (canvas, wcell_a, wcell_b,
                                &canvas->config.symbol_map.symbols2 [i],
                                &eval);

        if (eval.error [0] + eval.error [1] < best_eval.error [0] + best_eval.error [1])
        {
            best_symbol = i;
            best_eval = eval;
        }
    }

#ifdef HAVE_MMX_INTRINSICS
    /* Make FPU happy again */
    if (chafa_have_mmx ())
        leave_mmx ();
#endif

    g_assert (best_symbol >= 0);

    if (error_a_out)
        *error_a_out = best_eval.error [0];
    if (error_b_out)
        *error_b_out = best_eval.error [1];

    *sym_out = canvas->config.symbol_map.symbols2 [best_symbol].sym [0].c;
    *fg_col_out = best_eval.fg.col;
    *bg_col_out = best_eval.bg.col;
}

static void
pick_symbol_and_colors_fast (ChafaCanvas *canvas,
                             WorkCell *wcell,
                             gunichar *sym_out,
                             ChafaColor *fg_col_out,
                             ChafaColor *bg_col_out,
                             gint *error_out)
{
    ChafaColor color_pair [2];
    guint64 bitmap;
    ChafaCandidate candidates [N_CANDIDATES_MAX];
    SymbolEval eval [N_CANDIDATES_MAX];
    gint n_candidates = 0;
    gint best_candidate = 0;
    gint best_error = G_MAXINT;
    gint i;

    if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG
        || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG_BGFG)
    {
        color_pair [0] = canvas->fg_color;
        color_pair [1] = canvas->bg_color;
    }
    else
    {
        work_cell_get_contrasting_color_pair (wcell, color_pair);
    }

    bitmap = block_to_bitmap (wcell->pixels, color_pair);
    n_candidates = CLAMP (canvas->work_factor_int, 1, N_CANDIDATES_MAX);

    chafa_symbol_map_find_candidates (&canvas->config.symbol_map,
                                      bitmap,
                                      canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG
                                      ? FALSE : TRUE,  /* Consider inverted? */
                                      candidates, &n_candidates);

    g_assert (n_candidates > 0);

    for (i = 0; i < n_candidates; i++)
    {
        const ChafaSymbol *sym = &canvas->config.symbol_map.symbols [candidates [i].symbol_index];

        if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG)
        {
            eval [i].fg.col = canvas->fg_color;
            eval [i].bg.col = canvas->bg_color;
        }
        else
        {
            eval_symbol_colors (canvas, wcell, sym, &eval [i]);
        }

        eval_symbol_error (canvas, wcell, sym, &eval [i]);

        if (eval [i].error < best_error)
        {
            best_candidate = i;
            best_error = eval [i].error;
        }
    }

    *sym_out = canvas->config.symbol_map.symbols [candidates [best_candidate].symbol_index].c;
    *fg_col_out = eval [best_candidate].fg.col;
    *bg_col_out = eval [best_candidate].bg.col;

#ifdef HAVE_MMX_INTRINSICS
    /* Make FPU happy again */
    if (chafa_have_mmx ())
        leave_mmx ();
#endif

    if (error_out)
        *error_out = best_error;
}

static void
pick_symbol_and_colors_wide_fast (ChafaCanvas *canvas,
                                  WorkCell *wcell_a,
                                  WorkCell *wcell_b,
                                  gunichar *sym_out,
                                  ChafaColor *fg_col_out,
                                  ChafaColor *bg_col_out,
                                  gint *error_a_out,
                                  gint *error_b_out)
{
    ChafaColor color_pair [2];
    guint64 bitmaps [2];
    ChafaCandidate candidates [N_CANDIDATES_MAX];
    SymbolEval2 eval [N_CANDIDATES_MAX];
    gint n_candidates = 0;
    gint best_candidate = 0;
    gint best_error = G_MAXINT;
    gint i;

    if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG
        || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG_BGFG)
    {
        color_pair [0] = canvas->fg_color;
        color_pair [1] = canvas->bg_color;
    }
    else
    {
        ChafaColor color_pair_part [2] [2];

        work_cell_get_contrasting_color_pair (wcell_a, color_pair_part [0]);
        work_cell_get_contrasting_color_pair (wcell_b, color_pair_part [1]);

        color_pair [0] = chafa_color_average_2 (color_pair_part [0] [0], color_pair_part [1] [0]);
        color_pair [1] = chafa_color_average_2 (color_pair_part [0] [1], color_pair_part [1] [1]);
    }

    bitmaps [0] = block_to_bitmap (wcell_a->pixels, color_pair);
    bitmaps [1] = block_to_bitmap (wcell_b->pixels, color_pair);
    n_candidates = CLAMP (canvas->work_factor_int, 1, N_CANDIDATES_MAX);

    chafa_symbol_map_find_wide_candidates (&canvas->config.symbol_map,
                                           bitmaps,
                                           canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG
                                           ? FALSE : TRUE,  /* Consider inverted? */
                                           candidates, &n_candidates);

    g_assert (n_candidates > 0);

    for (i = 0; i < n_candidates; i++)
    {
        const ChafaSymbol2 *sym = &canvas->config.symbol_map.symbols2 [candidates [i].symbol_index];

        if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG)
        {
            eval [i].fg.col = canvas->fg_color;
            eval [i].bg.col = canvas->bg_color;
        }
        else
        {
            eval_symbol_colors_wide (canvas, wcell_a, wcell_b,
                                     &sym->sym [0],
                                     &sym->sym [1],
                                     &eval [i]);
        }

        eval_symbol_error_wide (canvas, wcell_a, wcell_b, sym, &eval [i]);

        if (eval [i].error [0] + eval [i].error [1] < best_error)
        {
            best_candidate = i;
            best_error = eval [i].error [0] + eval [i].error [1];
        }
    }

    *sym_out = canvas->config.symbol_map.symbols2 [candidates [best_candidate].symbol_index].sym [0].c;
    *fg_col_out = eval [best_candidate].fg.col;
    *bg_col_out = eval [best_candidate].bg.col;

#ifdef HAVE_MMX_INTRINSICS
    /* Make FPU happy again */
    if (chafa_have_mmx ())
        leave_mmx ();
#endif

    if (error_a_out)
        *error_a_out = eval [best_candidate].error [0];
    if (error_b_out)
        *error_b_out = eval [best_candidate].error [1];
}

static const ChafaColor *
get_palette_color (ChafaCanvas *canvas, gint index)
{
    if (index == CHAFA_PALETTE_INDEX_FG)
        return &canvas->fg_color;
    if (index == CHAFA_PALETTE_INDEX_BG)
        return &canvas->bg_color;
    if (index == CHAFA_PALETTE_INDEX_TRANSPARENT)
        return &canvas->bg_color;

    return chafa_get_palette_color_256 (index, canvas->config.color_space);
}

static void
apply_fill (ChafaCanvas *canvas, const WorkCell *wcell, ChafaCanvasCell *cell)
{
    ChafaColor mean;
    ChafaColor col [3];
    ChafaColorCandidates ccand;
    ChafaCandidate sym_cand;
    gint n_sym_cands = 1;
    gint i, best_i = 0;
    gint error, best_error = G_MAXINT;

    if (canvas->config.fill_symbol_map.n_symbols == 0)
        return;

    calc_mean_color (wcell->pixels, &mean);

    if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_TRUECOLOR)
    {
        cell->bg_color = cell->fg_color = chafa_pack_color (&mean);
        chafa_symbol_map_find_fill_candidates (&canvas->config.fill_symbol_map, 0,
                                               FALSE,  /* Consider inverted? */
                                               &sym_cand, &n_sym_cands);
        goto done;
    }
    else if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_256
             || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_240
             || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_16)
    {
        chafa_palette_lookup_nearest (&canvas->palette, canvas->config.color_space,
                                      &mean, &ccand);
    }
    else if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG_BGFG
             || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG)
    {
        ccand.index [0] = CHAFA_PALETTE_INDEX_FG;
        ccand.index [1] = CHAFA_PALETTE_INDEX_BG;
    }
    else
    {
        g_assert_not_reached ();
    }

    col [0] = *get_palette_color (canvas, ccand.index [0]);
    col [1] = *get_palette_color (canvas, ccand.index [1]);

    /* In FGBG modes, background and transparency is the same thing. Make
     * sure we have two opaque colors for correct interpolation. */
    if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG_BGFG
        || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG)
        col [1].ch [3] = 0xff;

    /* Make the primary color correspond to cell's BG pen, so mostly transparent
     * cells will get a transparent BG; terminals typically don't support
     * transparency in the FG pen. BG is also likely to cover a greater area. */
    for (i = 0; i <= 64; i++)
    {
        col [2].ch [0] = (col [0].ch [0] * (64 - i) + col [1].ch [0] * i) / 64;
        col [2].ch [1] = (col [0].ch [1] * (64 - i) + col [1].ch [1] * i) / 64;
        col [2].ch [2] = (col [0].ch [2] * (64 - i) + col [1].ch [2] * i) / 64;
        col [2].ch [3] = (col [0].ch [3] * (64 - i) + col [1].ch [3] * i) / 64;

        error = chafa_color_diff_slow (&mean, &col [2], canvas->config.color_space);
        if (error < best_error)
        {
            /* In FGBG mode there's no way to invert or set the BG color, so
             * assign the primary color to FG pen instead. */
            best_i = (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG ? 64 - i : i);
            best_error = error;
        }
    }

    chafa_symbol_map_find_fill_candidates (&canvas->config.fill_symbol_map, best_i,
                                           canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG
                                           ? FALSE : TRUE,  /* Consider inverted? */
                                           &sym_cand, &n_sym_cands);

    /* If we end up with a featureless symbol (space or fill), make
     * FG color equal to BG. */
    if (best_i == 0)
        ccand.index [1] = ccand.index [0];
    else if (best_i == 64)
        ccand.index [0] = ccand.index [1];

    if (sym_cand.is_inverted)
    {
        cell->fg_color = ccand.index [0];
        cell->bg_color = ccand.index [1];
    }
    else
    {
        cell->fg_color = ccand.index [1];
        cell->bg_color = ccand.index [0];
    }

done:
    cell->c = canvas->config.fill_symbol_map.symbols [sym_cand.symbol_index].c;
}

static gint
update_cell (ChafaCanvas *canvas, WorkCell *work_cell, ChafaCanvasCell *cell_out)
{
    gunichar sym = 0;
    ChafaColorCandidates ccand;
    ChafaColor fg_col, bg_col;
    gint sym_error;

    if (canvas->config.symbol_map.n_symbols == 0)
        return SYMBOL_ERROR_MAX;

    if (canvas->work_factor_int >= 8)
        pick_symbol_and_colors_slow (canvas, work_cell, &sym, &fg_col, &bg_col, &sym_error);
    else
        pick_symbol_and_colors_fast (canvas, work_cell, &sym, &fg_col, &bg_col, &sym_error);

    cell_out->c = sym;

    if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_256
        || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_240
        || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_16
        || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG_BGFG)
    {
        chafa_palette_lookup_nearest (&canvas->palette, canvas->config.color_space, &fg_col, &ccand);
        cell_out->fg_color = ccand.index [0];
        chafa_palette_lookup_nearest (&canvas->palette, canvas->config.color_space, &bg_col, &ccand);
        cell_out->bg_color = ccand.index [0];
    }
    else
    {
        cell_out->fg_color = chafa_pack_color (&fg_col);
        cell_out->bg_color = chafa_pack_color (&bg_col);
    }

    return sym_error;
}

static void
update_cells_wide (ChafaCanvas *canvas, WorkCell *work_cell_a, WorkCell *work_cell_b,
                   ChafaCanvasCell *cell_a_out, ChafaCanvasCell *cell_b_out,
                   gint *error_a_out, gint *error_b_out)
{
    gunichar sym = 0;
    ChafaColorCandidates ccand;
    ChafaColor fg_col, bg_col;

    *error_a_out = *error_b_out = SYMBOL_ERROR_MAX;

    if (canvas->config.symbol_map.n_symbols2 == 0)
        return;

    if (canvas->work_factor_int >= 8)
        pick_symbol_and_colors_wide_slow (canvas, work_cell_a, work_cell_b,
                                          &sym, &fg_col, &bg_col,
                                          error_a_out, error_b_out);
    else
        pick_symbol_and_colors_wide_fast (canvas, work_cell_a, work_cell_b,
                                          &sym, &fg_col, &bg_col,
                                          error_a_out, error_b_out);

    cell_a_out->c = sym;
    cell_b_out->c = 0;

    if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_256
        || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_240
        || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_16
        || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG_BGFG)
    {
        chafa_palette_lookup_nearest (&canvas->palette, canvas->config.color_space, &fg_col, &ccand);
        cell_a_out->fg_color = cell_b_out->fg_color = ccand.index [0];
        chafa_palette_lookup_nearest (&canvas->palette, canvas->config.color_space, &bg_col, &ccand);
        cell_a_out->bg_color = cell_b_out->bg_color = ccand.index [0];
    }
    else
    {
        cell_a_out->fg_color = cell_b_out->fg_color = chafa_pack_color (&fg_col);
        cell_a_out->bg_color = cell_b_out->bg_color = chafa_pack_color (&bg_col);
    }
}

/* Number of entries in our cell ring buffer. This allows us to do lookback
 * and replace single-cell symbols with double-cell ones if it improves
 * the error value. */
#define N_BUF_CELLS 4

/* Calculate index after positive or negative wraparound(s) */
#define buf_cell_index(i) (((i) + N_BUF_CELLS * 64) % N_BUF_CELLS)

static void
update_cells_row (ChafaCanvas *canvas, gint row)
{
    ChafaCanvasCell *cells;
    WorkCell work_cells [N_BUF_CELLS];
    gint cell_errors [N_BUF_CELLS];
    gint cx, cy;

    cells = &canvas->cells [row * canvas->config.width];
    cy = row;

    for (cx = 0; cx < canvas->config.width; cx++)
    {
        gint buf_index = cx % N_BUF_CELLS;
        WorkCell *wcell = &work_cells [buf_index];
        ChafaCanvasCell wide_cells [2];
        gint wide_cell_errors [2];

        memset (&cells [cx], 0, sizeof (cells [cx]));
        cells [cx].c = ' ';

        work_cell_init (wcell, canvas, cx, cy);
        cell_errors [buf_index] = update_cell (canvas, wcell, &cells [cx]);

        /* Try wide symbol */

        /* FIXME: If we're overlapping the rightmost half of a wide symbol,
         * try to revert it to two regular symbols and overwrite the rightmost
         * one. */

        if (cx >= 1 && cells [cx - 1].c != 0)
        {
            gint wide_buf_index [2];

            wide_buf_index [0] = buf_cell_index (cx - 1);
            wide_buf_index [1] = buf_index;

            update_cells_wide (canvas,
                               &work_cells [wide_buf_index [0]],
                               &work_cells [wide_buf_index [1]],
                               &wide_cells [0],
                               &wide_cells [1],
                               &wide_cell_errors [0],
                               &wide_cell_errors [1]);

            if (wide_cell_errors [0] + wide_cell_errors [1] <
                cell_errors [wide_buf_index [0]] + cell_errors [wide_buf_index [1]])
            {
                cells [cx - 1] = wide_cells [0];
                cells [cx] = wide_cells [1]; cells [cx].c = 0;
                cell_errors [wide_buf_index [0]] = wide_cell_errors [0];
                cell_errors [wide_buf_index [1]] = wide_cell_errors [1];
            }
        }

        /* If we produced a featureless cell, try fill */

        /* FIXME: Check popcount == 0 or == 64 instead of symbol char */
        if (cells [cx].c != 0 && (cells [cx].c == ' ' || cells [cx].c == 0x2588
                                  || cells [cx].fg_color == cells [cx].bg_color))
        {
            apply_fill (canvas, wcell, &cells [cx]);
        }
    }
}

typedef struct
{
    gint row;
}
CellBuildWork;

static void
cell_build_worker (CellBuildWork *work, ChafaCanvas *canvas)
{
    update_cells_row (canvas, work->row);
    g_slice_free (CellBuildWork, work);
}

static void
update_cells (ChafaCanvas *canvas)
{
    GThreadPool *thread_pool = g_thread_pool_new ((GFunc) cell_build_worker,
                                                  canvas,
                                                  g_get_num_processors (),
                                                  FALSE,
                                                  NULL);
    gint cy;

    for (cy = 0; cy < canvas->config.height; cy++)
    {
        CellBuildWork *work = g_slice_new (CellBuildWork);
        work->row = cy;
        g_thread_pool_push (thread_pool, work, NULL);
    }

    g_thread_pool_free (thread_pool, FALSE, TRUE);
}

static void
update_display_colors (ChafaCanvas *canvas)
{
    ChafaColor fg_col;
    ChafaColor bg_col;

    chafa_unpack_color (canvas->config.fg_color_packed_rgb, &fg_col);
    chafa_unpack_color (canvas->config.bg_color_packed_rgb, &bg_col);

    if (canvas->config.color_space == CHAFA_COLOR_SPACE_DIN99D)
    {
        chafa_color_rgb_to_din99d (&fg_col, &canvas->fg_color);
        chafa_color_rgb_to_din99d (&bg_col, &canvas->bg_color);
    }
    else
    {
        canvas->fg_color = fg_col;
        canvas->bg_color = bg_col;
    }

    canvas->fg_color.ch [3] = 0xff;
    canvas->bg_color.ch [3] = 0x00;
}

static void
maybe_clear (ChafaCanvas *canvas)
{
    gint i;

    if (!canvas->needs_clear)
        return;

    for (i = 0; i < canvas->config.width * canvas->config.height; i++)
    {
        ChafaCanvasCell *cell = &canvas->cells [i];

        memset (cell, 0, sizeof (*cell));
        cell->c = ' ';
    }
}

static void
setup_palette (ChafaCanvas *canvas)
{
    ChafaPaletteType pal_type = CHAFA_PALETTE_TYPE_DYNAMIC_256;
    ChafaColor fg_col;
    ChafaColor bg_col;

    chafa_unpack_color (canvas->config.fg_color_packed_rgb, &fg_col);
    chafa_unpack_color (canvas->config.bg_color_packed_rgb, &bg_col);

    fg_col.ch [3] = 0xff;
    bg_col.ch [3] = 0x00;

    switch (chafa_canvas_config_get_canvas_mode (&canvas->config))
    {
        case CHAFA_CANVAS_MODE_TRUECOLOR:
            pal_type = CHAFA_PALETTE_TYPE_DYNAMIC_256;
            break;

        case CHAFA_CANVAS_MODE_INDEXED_256:
            pal_type = CHAFA_PALETTE_TYPE_FIXED_256;
            break;

        case CHAFA_CANVAS_MODE_INDEXED_240:
            pal_type = CHAFA_PALETTE_TYPE_FIXED_240;
            break;

        case CHAFA_CANVAS_MODE_INDEXED_16:
            pal_type = CHAFA_PALETTE_TYPE_FIXED_16;
            break;

        case CHAFA_CANVAS_MODE_FGBG_BGFG:
        case CHAFA_CANVAS_MODE_FGBG:
            pal_type = CHAFA_PALETTE_TYPE_FIXED_FGBG;
            break;

        case CHAFA_CANVAS_MODE_MAX:
            g_assert_not_reached ();
    }

    chafa_palette_init (&canvas->palette, pal_type);

    chafa_palette_set_color (&canvas->palette, CHAFA_PALETTE_INDEX_FG, &fg_col);
    chafa_palette_set_color (&canvas->palette, CHAFA_PALETTE_INDEX_BG, &bg_col);

    chafa_palette_set_alpha_threshold (&canvas->palette, canvas->config.alpha_threshold);
    chafa_palette_set_transparent_index (&canvas->palette, CHAFA_PALETTE_INDEX_TRANSPARENT);
}

/**
 * chafa_canvas_new:
 * @config: Configuration to use or %NULL for hardcoded defaults
 *
 * Creates a new canvas with the specified configuration. The
 * canvas makes a private copy of the configuration, so it will
 * not be affected by subsequent changes.
 *
 * Returns: The new canvas
 **/
ChafaCanvas *
chafa_canvas_new (const ChafaCanvasConfig *config)
{
    ChafaCanvas *canvas;
    gdouble dither_intensity = 1.0;

    if (config)
    {
        g_return_val_if_fail (config->width > 0, NULL);
        g_return_val_if_fail (config->height > 0, NULL);
    }

    chafa_init ();

    canvas = g_new0 (ChafaCanvas, 1);

    if (config)
        chafa_canvas_config_copy_contents (&canvas->config, config);
    else
        chafa_canvas_config_init (&canvas->config);

    canvas->refs = 1;

    if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_SYMBOLS)
    {
        /* ANSI art */
        canvas->width_pixels = canvas->config.width * CHAFA_SYMBOL_WIDTH_PIXELS;
        canvas->height_pixels = canvas->config.height * CHAFA_SYMBOL_HEIGHT_PIXELS;
    }
    else
    {
        /* Sixels */
        canvas->width_pixels = canvas->config.width * canvas->config.cell_width;
        canvas->height_pixels = canvas->config.height * canvas->config.cell_height;

        /* Ensure height is the biggest multiple of 6 that will fit
         * in our cells. We don't want a fringe going outside our
         * bottom cell. */
        canvas->height_pixels -= canvas->height_pixels % 6;
    }

    canvas->pixels = NULL;
    canvas->cells = g_new (ChafaCanvasCell, canvas->config.width * canvas->config.height);
    canvas->work_factor_int = canvas->config.work_factor * 10 + 0.5;
    canvas->needs_clear = TRUE;
    canvas->have_alpha = FALSE;

    chafa_symbol_map_prepare (&canvas->config.symbol_map);
    chafa_symbol_map_prepare (&canvas->config.fill_symbol_map);

    /* In truecolor mode we don't support any fancy color spaces for now, since
     * we'd have to convert back to RGB space when emitting control codes, and
     * the code for that has yet to be written. In palette modes we just use
     * the palette mappings.
     *
     * There is also no reason to dither in truecolor mode, _unless_ we're
     * producing sixels, which quantize to a dynamic palette. */
    if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_TRUECOLOR
        && canvas->config.pixel_mode == CHAFA_PIXEL_MODE_SYMBOLS)
    {
        canvas->config.color_space = CHAFA_COLOR_SPACE_RGB;
        canvas->config.dither_mode = CHAFA_DITHER_MODE_NONE;
    }

    if (canvas->config.dither_mode == CHAFA_DITHER_MODE_ORDERED)
    {
        switch (canvas->config.canvas_mode)
        {
            case CHAFA_CANVAS_MODE_TRUECOLOR:
            case CHAFA_CANVAS_MODE_INDEXED_256:
            case CHAFA_CANVAS_MODE_INDEXED_240:
                dither_intensity = DITHER_BASE_INTENSITY_256C;
                break;
            case CHAFA_CANVAS_MODE_INDEXED_16:
                dither_intensity = DITHER_BASE_INTENSITY_16C;
                break;
            case CHAFA_CANVAS_MODE_FGBG:
            case CHAFA_CANVAS_MODE_FGBG_BGFG:
                dither_intensity = DITHER_BASE_INTENSITY_FGBG;
                break;
            default:
                g_assert_not_reached ();
                break;
        }
    }

    chafa_dither_init (&canvas->dither, canvas->config.dither_mode,
                       dither_intensity * canvas->config.dither_intensity,
                       canvas->config.dither_grain_width,
                       canvas->config.dither_grain_height);

    update_display_colors (canvas);
    setup_palette (canvas);

    return canvas;
}

/**
 * chafa_canvas_new_similar:
 * @orig: Canvas to copy configuration from
 *
 * Creates a new canvas configured similarly to @orig.
 *
 * Returns: The new canvas
 **/
ChafaCanvas *
chafa_canvas_new_similar (ChafaCanvas *orig)
{
    ChafaCanvas *canvas;

    g_return_val_if_fail (orig != NULL, NULL);

    canvas = g_new (ChafaCanvas, 1);
    memcpy (canvas, orig, sizeof (*canvas));
    canvas->refs = 1;

    chafa_canvas_config_copy_contents (&canvas->config, &orig->config);

    canvas->pixels = NULL;
    canvas->cells = g_new (ChafaCanvasCell, canvas->config.width * canvas->config.height);
    canvas->needs_clear = TRUE;

    chafa_dither_copy (&orig->dither, &canvas->dither);

    return canvas;
}

/**
 * chafa_canvas_ref:
 * @canvas: Canvas to add a reference to
 *
 * Adds a reference to @canvas.
 **/
void
chafa_canvas_ref (ChafaCanvas *canvas)
{
    gint refs;

    g_return_if_fail (canvas != NULL);
    refs = g_atomic_int_get (&canvas->refs);
    g_return_if_fail (refs > 0);

    g_atomic_int_inc (&canvas->refs);
}

/**
 * chafa_canvas_unref:
 * @canvas: Canvas to remove a reference from
 *
 * Removes a reference from @canvas. When remaining references drops to
 * zero, the canvas is freed and can no longer be used.
 **/
void
chafa_canvas_unref (ChafaCanvas *canvas)
{
    gint refs;

    g_return_if_fail (canvas != NULL);
    refs = g_atomic_int_get (&canvas->refs);
    g_return_if_fail (refs > 0);

    if (g_atomic_int_dec_and_test (&canvas->refs))
    {
        chafa_canvas_config_deinit (&canvas->config);

        if (canvas->sixel_canvas)
        {
            chafa_sixel_canvas_destroy (canvas->sixel_canvas);
            canvas->sixel_canvas = NULL;
        }

        chafa_dither_deinit (&canvas->dither);
        chafa_palette_deinit (&canvas->palette);
        g_free (canvas->pixels);
        g_free (canvas->cells);
        g_free (canvas);
    }
}

/**
 * chafa_canvas_peek_config:
 * @canvas: Canvas whose configuration to inspect
 *
 * Returns a pointer to the configuration belonging to @canvas.
 * This can be inspected using the #ChafaCanvasConfig getter
 * functions, but not changed.
 *
 * Returns: A pointer to the canvas' immutable configuration
 **/
const ChafaCanvasConfig *
chafa_canvas_peek_config (ChafaCanvas *canvas)
{
    g_return_val_if_fail (canvas != NULL, NULL);
    g_return_val_if_fail (canvas->refs > 0, NULL);

    return &canvas->config;
}

/**
 * chafa_canvas_draw_all_pixels:
 * @canvas: Canvas whose pixel data to replace
 * @src_pixel_type: Pixel format of @src_pixels
 * @src_pixels: Pointer to the start of source pixel memory
 * @src_width: Width in pixels of source pixel data
 * @src_height: Height in pixels of source pixel data
 * @src_rowstride: Number of bytes between the start of each pixel row
 *
 * Replaces pixel data of @canvas with a copy of that found at @src_pixels,
 * which must be in one of the formats supported by #ChafaPixelType.
 *
 * Since: 1.2
 **/
void
chafa_canvas_draw_all_pixels (ChafaCanvas *canvas, ChafaPixelType src_pixel_type,
                              const guint8 *src_pixels,
                              gint src_width, gint src_height, gint src_rowstride)
{
    g_return_if_fail (canvas != NULL);
    g_return_if_fail (canvas->refs > 0);
    g_return_if_fail (src_pixel_type < CHAFA_PIXEL_MAX);
    g_return_if_fail (src_pixels != NULL);
    g_return_if_fail (src_width >= 0);
    g_return_if_fail (src_height >= 0);

    if (src_width == 0 || src_height == 0)
        return;

    if (canvas->pixels)
    {
        g_free (canvas->pixels);
        canvas->pixels = NULL;
    }

    if (canvas->sixel_canvas)
    {
        chafa_sixel_canvas_destroy (canvas->sixel_canvas);
        canvas->sixel_canvas = NULL;
    }

    if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_SYMBOLS)
    {
        /* Symbol mode */

        canvas->pixels = g_new (ChafaPixel, canvas->width_pixels * canvas->height_pixels);

        chafa_prepare_pixel_data_for_symbols (&canvas->palette, &canvas->dither,
                                              canvas->config.color_space,
                                              canvas->config.preprocessing_enabled,
                                              canvas->work_factor_int,
                                              src_pixel_type,
                                              src_pixels,
                                              src_width, src_height,
                                              src_rowstride,
                                              canvas->pixels,
                                              canvas->width_pixels, canvas->height_pixels);

        if (canvas->config.alpha_threshold == 0)
            canvas->have_alpha = FALSE;

        update_cells (canvas);
        canvas->needs_clear = FALSE;

        g_free (canvas->pixels);
        canvas->pixels = NULL;
    }
    else
    {
        /* Sixel mode */

        canvas->palette.alpha_threshold = canvas->config.alpha_threshold;
        canvas->sixel_canvas = chafa_sixel_canvas_new (canvas->width_pixels,
                                                       canvas->height_pixels,
                                                       canvas->config.color_space,
                                                       &canvas->palette,
                                                       &canvas->dither);
        chafa_sixel_canvas_draw_all_pixels (canvas->sixel_canvas,
                                            src_pixel_type,
                                            src_pixels,
                                            src_width, src_height,
                                            src_rowstride);
    }
}

/**
 * chafa_canvas_set_contents_rgba8:
 * @canvas: Canvas whose pixel data to replace
 * @src_pixels: Pointer to the start of source pixel memory
 * @src_width: Width in pixels of source pixel data
 * @src_height: Height in pixels of source pixel data
 * @src_rowstride: Number of bytes between the start of each pixel row
 *
 * Replaces pixel data of @canvas with a copy of that found at @src_pixels.
 * The source data must be in packed 8-bits-per-channel RGBA format. The
 * alpha value is expressed as opacity (0xff is opaque) and is not
 * premultiplied.
 *
 * Deprecated: 1.2: Use chafa_canvas_draw_all_pixels() instead.
 **/
void
chafa_canvas_set_contents_rgba8 (ChafaCanvas *canvas, const guint8 *src_pixels,
                                 gint src_width, gint src_height, gint src_rowstride)
{
    chafa_canvas_draw_all_pixels (canvas, CHAFA_PIXEL_RGBA8_UNASSOCIATED,
                                  src_pixels, src_width, src_height, src_rowstride);
}

/**
 * chafa_canvas_build_ansi:
 * @canvas: The canvas to generate an ANSI character representation of
 *
 * Builds a UTF-8 string of ANSI sequences and symbols representing
 * the canvas' current contents. This can e.g. be printed to a terminal.
 * The exact choice of escape sequences and symbols, dimensions, etc. is
 * determined by the configuration assigned to @canvas on its creation.
 *
 * All output lines except for the last one will end in a newline.
 *
 * Returns: A UTF-8 string of ANSI sequences and symbols
 **/
GString *
chafa_canvas_build_ansi (ChafaCanvas *canvas)
{
    GString *str;

    g_return_val_if_fail (canvas != NULL, NULL);
    g_return_val_if_fail (canvas->refs > 0, NULL);

    if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_SYMBOLS)
    {
        maybe_clear (canvas);
        str = chafa_canvas_print (canvas, NULL);
    }
    else
    {
        /* Sixel mode */

        str = g_string_new ("\x1bP0;1;0q");
        g_string_append_printf (str, "\"1;1;%d;%d", canvas->width_pixels, canvas->height_pixels);
        chafa_sixel_canvas_build_ansi (canvas->sixel_canvas, str);
        g_string_append (str, "\x1b\\");
    }

    return str;
}
