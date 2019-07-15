/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018 Hans Petter Jansson
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
#include "chafa/chafa.h"
#include "chafa/chafa-private.h"

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
 * You can draw an image to the canvas using chafa_canvas_set_contents_rgba8 ()
 * and create an ANSI text representation of the canvas' current contents
 * using chafa_canvas_build_ansi ().
 **/

/* Fixed point multiplier */
#define FIXED_MULT 16384

/* Max candidates to consider in pick_symbol_and_colors_fast(). This is also
 * limited by a similar constant in chafa-symbol-map.c */
#define N_CANDIDATES_MAX 8

/* See rgb_to_intensity_fast () */
#define INTENSITY_MAX (256 * 8)

/* Normalization: Percentage of pixels to discard at extremes of histogram */
#define INDEXED_16_CROP_PCT 5
#define INDEXED_2_CROP_PCT  20

/* Dithering */
#define DITHER_GRAIN_WIDTH 4
#define DITHER_GRAIN_HEIGHT 4
#define DITHER_BASE_INTENSITY_FGBG 1.0
#define DITHER_BASE_INTENSITY_16C  0.25
#define DITHER_BASE_INTENSITY_256C 0.1
#define BAYER_MATRIX_DIM_SHIFT 4
#define BAYER_MATRIX_DIM (1 << (BAYER_MATRIX_DIM_SHIFT))
#define BAYER_MATRIX_SIZE ((BAYER_MATRIX_DIM) * (BAYER_MATRIX_DIM))

typedef struct
{
    gint32 c [INTENSITY_MAX];

    /* Lower and upper bounds */
    gint32 min, max;
}
Histogram;

struct ChafaCanvasCell
{
    gunichar c;

    /* Colors can be either packed RGBA or index */
    guint32 fg_color;
    guint32 bg_color;
};

struct ChafaCanvas
{
    gint refs;

    gint width_pixels, height_pixels;
    ChafaPixel *pixels;
    ChafaCanvasCell *cells;
    guint have_alpha : 1;
    guint needs_clear : 1;
    ChafaColor fg_color;
    ChafaColor bg_color;
    guint work_factor_int;

    ChafaCanvasConfig config;

    /* Used when setting pixel data */
    const guint8 *src_pixels;
    Histogram *hist;
    gint src_width, src_height, src_rowstride;
    gint have_alpha_int;
    gint dither_grain_width_shift, dither_grain_height_shift;

    /* Set if we're doing bayer dithering */
    gint *bayer_matrix;
    gint bayer_size_shift;
};

typedef struct
{
    ChafaPixel fg;
    ChafaPixel bg;
    gint error;
}
SymbolEval;

/* block_out must point to CHAFA_SYMBOL_N_PIXELS-element array */
static void
fetch_canvas_pixel_block (ChafaCanvas *canvas, gint cx, gint cy, ChafaPixel *block_out)
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
            block_out [i++] = *p0;
    }
}

static void
threshold_alpha (ChafaCanvas *canvas, ChafaColor *color)
{
    if (color->ch [3] < canvas->config.alpha_threshold)
        color->ch [3] = 0x00;
    else
        color->ch [3] = 0xff;
}

static void
calc_mean_color (const ChafaPixel *block, ChafaColor *color_out)
{
    ChafaColor accum = { 0 };
    gint i;

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        chafa_color_add (&accum, &block->col);
        block++;
    }

    chafa_color_div_scalar (&accum, CHAFA_SYMBOL_N_PIXELS);
    *color_out = accum;
}

static gint
find_dominant_channel (const ChafaPixel *block)
{
    gint16 min [4] = { G_MAXINT16, G_MAXINT16, G_MAXINT16, G_MAXINT16 };
    gint16 max [4] = { G_MININT16, G_MININT16, G_MININT16, G_MININT16 };
    gint16 range [4];
    gint ch, best_ch;
    gint i;

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        for (ch = 0; ch < 4; ch++)
        {
            if (block [i].col.ch [ch] < min [ch])
                min [ch] = block [i].col.ch [ch];
            if (block [i].col.ch [ch] > max [ch])
                max [ch] = block [i].col.ch [ch];
        }
    }

    for (ch = 0; ch < 4; ch++)
        range [ch] = max [ch] - min [ch];

    best_ch = 0;
    for (ch = 1; ch < 4; ch++)
        if (range [ch] > range [best_ch])
            best_ch = ch;

    return best_ch;
}

static void
sort_block_by_channel (ChafaPixel *block, gint ch)
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

        for (i = gap; i < CHAFA_SYMBOL_N_PIXELS; i++)
        {
            ChafaPixel ptemp = block [i];

            for (j = i; j >= gap && block [j - gap].col.ch [ch] > ptemp.col.ch [ch]; j -= gap)
            {
                block [j] = block [j - gap];
            }

            block [j] = ptemp;
        }

        /* After gap == 1 the array is always left sorted */
        if (gap == 1)
            break;
    }
}

/* colors_out must point to an array of two elements */
static void
pick_two_colors (const ChafaPixel *block, ChafaColor *colors_out)
{
    ChafaPixel sorted_colors [CHAFA_SYMBOL_N_PIXELS];
    gint best_ch;

    best_ch = find_dominant_channel (block);

    /* FIXME: An array of index bytes might be faster due to smaller
     * elements, but the indirection adds overhead so it'd have to be
     * benchmarked. */

    memcpy (sorted_colors, block, sizeof (sorted_colors));
    sort_block_by_channel (sorted_colors, best_ch);

    /* Choose two colors by median cut */

    colors_out [0] = sorted_colors [CHAFA_SYMBOL_N_PIXELS / 4].col;
    colors_out [1] = sorted_colors [(CHAFA_SYMBOL_N_PIXELS * 3) / 4].col;
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

static void
calc_colors_plain (const ChafaPixel *block, ChafaColor *cols, const guint8 *cov)
{
    const gint16 *in_s16 = (const gint16 *) block;
    gint i;

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        gint16 *out_s16 = (gint16 *) (cols + *cov++);

        *out_s16++ += *in_s16++;
        *out_s16++ += *in_s16++;
        *out_s16++ += *in_s16++;
        *out_s16++ += *in_s16++;
    }
}

static void
eval_symbol_colors (const ChafaPixel *block, const ChafaSymbol *sym, SymbolEval *eval)
{
    const guint8 *covp = (guint8 *) &sym->coverage [0];
    ChafaColor cols [2] = { 0 };

#ifdef HAVE_MMX_INTRINSICS
    if (chafa_have_mmx ())
        calc_colors_mmx (block, cols, covp);
    else
#endif
        calc_colors_plain (block, cols, covp);

    eval->fg.col = cols [1];
    eval->bg.col = cols [0];

    if (sym->fg_weight > 1)
        chafa_color_div_scalar (&eval->fg.col, sym->fg_weight);

    if (sym->bg_weight > 1)
        chafa_color_div_scalar (&eval->bg.col, sym->bg_weight);
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
eval_symbol_error (ChafaCanvas *canvas, const ChafaPixel *block,
                   const ChafaSymbol *sym, SymbolEval *eval)
{
    const guint8 *covp = (guint8 *) &sym->coverage [0];
    ChafaColor cols [2] = { 0 };
    gint error;

    cols [0] = eval->bg.col;
    cols [1] = eval->fg.col;

    if (canvas->have_alpha)
    {
        error = calc_error_with_alpha (block, cols, covp, canvas->config.color_space);
    }
    else
    {
#ifdef HAVE_SSE41_INTRINSICS
        if (chafa_have_sse41 ())
            error = calc_error_sse41 (block, cols, covp);
        else
#endif
            error = calc_error_plain (block, cols, covp);
    }

    eval->error = error;
}

static void
pick_symbol_and_colors_slow (ChafaCanvas *canvas,
                             const ChafaPixel *block,
                             gunichar *sym_out,
                             ChafaColor *fg_col_out,
                             ChafaColor *bg_col_out,
                             gint *error_out)
{
    SymbolEval eval [CHAFA_N_SYMBOLS_MAX] = { 0 };
    gint n;
    gint i;

    for (i = 0; canvas->config.symbol_map.symbols [i].c != 0; i++)
    {
        eval [i].error = G_MAXINT;

        /* FIXME: Always evaluate space so we get fallback colors */

        if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG)
        {
            eval [i].fg.col = canvas->fg_color;
            eval [i].bg.col = canvas->bg_color;
        }
        else
        {
            ChafaColorCandidates ccand;
            ChafaColor fg_col, bg_col;

            eval_symbol_colors (block, &canvas->config.symbol_map.symbols [i], &eval [i]);

            /* Threshold alpha */

            threshold_alpha (canvas, &eval [i].fg.col);
            threshold_alpha (canvas, &eval [i].bg.col);

            fg_col = eval [i].fg.col;
            bg_col = eval [i].bg.col;

            /* Pick palette colors before error evaluation; this improves
             * fine detail fidelity slightly. */

            if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_16)
            {
                chafa_pick_color_16 (&eval [i].fg.col, canvas->config.color_space, &ccand);
                fg_col = *chafa_get_palette_color_256 (ccand.index [0], canvas->config.color_space);
                chafa_pick_color_16 (&eval [i].bg.col, canvas->config.color_space, &ccand);
                bg_col = *chafa_get_palette_color_256 (ccand.index [0], canvas->config.color_space);
            }
            else if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_240)
            {
                chafa_pick_color_240 (&eval [i].fg.col, canvas->config.color_space, &ccand);
                fg_col = *chafa_get_palette_color_256 (ccand.index [0], canvas->config.color_space);
                chafa_pick_color_240 (&eval [i].bg.col, canvas->config.color_space, &ccand);
                bg_col = *chafa_get_palette_color_256 (ccand.index [0], canvas->config.color_space);
            }
            else if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_256)
            {
                chafa_pick_color_256 (&eval [i].fg.col, canvas->config.color_space, &ccand);
                fg_col = *chafa_get_palette_color_256 (ccand.index [0], canvas->config.color_space);
                chafa_pick_color_256 (&eval [i].bg.col, canvas->config.color_space, &ccand);
                bg_col = *chafa_get_palette_color_256 (ccand.index [0], canvas->config.color_space);
            }

            /* FIXME: The logic here seems overly complicated */
            if (canvas->config.canvas_mode != CHAFA_CANVAS_MODE_TRUECOLOR)
            {
                /* Transfer mean alpha over so we can use it later */

                fg_col.ch [3] = eval [i].fg.col.ch [3];
                bg_col.ch [3] = eval [i].bg.col.ch [3];

                eval [i].fg.col = fg_col;
                eval [i].bg.col = bg_col;
            }
        }

        eval_symbol_error (canvas, block, &canvas->config.symbol_map.symbols [i], &eval [i]);
    }

#ifdef HAVE_MMX_INTRINSICS
    /* Make FPU happy again */
    if (chafa_have_mmx ())
        leave_mmx ();
#endif

    if (error_out)
        *error_out = eval [0].error;

    for (i = 0, n = 0; canvas->config.symbol_map.symbols [i].c != 0; i++)
    {
        if ((eval [i].fg.col.ch [0] != eval [i].bg.col.ch [0]
             || eval [i].fg.col.ch [1] != eval [i].bg.col.ch [1]
             || eval [i].fg.col.ch [2] != eval [i].bg.col.ch [2])
            && eval [i].error < eval [n].error)
        {
            n = i;
            if (error_out)
                *error_out = eval [i].error;
        }
    }

    *sym_out = canvas->config.symbol_map.symbols [n].c;
    *fg_col_out = eval [n].fg.col;
    *bg_col_out = eval [n].bg.col;
}

static void
pick_symbol_and_colors_fast (ChafaCanvas *canvas,
                             const ChafaPixel *block,
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
        pick_two_colors (block, color_pair);
    }

    bitmap = block_to_bitmap (block, color_pair);
    n_candidates = CLAMP (canvas->work_factor_int, 1, N_CANDIDATES_MAX);

    chafa_symbol_map_find_candidates (&canvas->config.symbol_map,
                                      bitmap,
                                      canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG
                                      ? FALSE : TRUE,  /* Consider inverted? */
                                      candidates, &n_candidates);

    g_assert (n_candidates > 0);

    if (n_candidates == 1)
    {
        if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG)
        {
            eval [0].fg.col = canvas->fg_color;
            eval [0].bg.col = canvas->bg_color;
        }
        else
        {
            eval_symbol_colors (block,
                                &canvas->config.symbol_map.symbols [candidates [0].symbol_index],
                                &eval [0]);
        }
    }
    else
    {
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
                eval_symbol_colors (block, sym, &eval [i]);
            }

            eval_symbol_error (canvas, block, sym, &eval [i]);

            if (eval [i].error < best_error)
            {
                best_candidate = i;
                best_error = eval [i].error;
            }
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
apply_fill (ChafaCanvas *canvas, const ChafaPixel *block, ChafaCanvasCell *cell)
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

    calc_mean_color (block, &mean);

    if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_TRUECOLOR)
    {
        cell->bg_color = cell->fg_color = chafa_pack_color (&mean);
        chafa_symbol_map_find_fill_candidates (&canvas->config.fill_symbol_map, 0,
                                               FALSE,  /* Consider inverted? */
                                               &sym_cand, &n_sym_cands);
        goto done;
    }
    else if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_256)
    {
        chafa_pick_color_256 (&mean, canvas->config.color_space, &ccand);
    }
    else if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_240)
    {
        chafa_pick_color_240 (&mean, canvas->config.color_space, &ccand);
    }
    else if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_16)
    {
        chafa_pick_color_16 (&mean, canvas->config.color_space, &ccand);
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

static void
update_cells_row (ChafaCanvas *canvas, gint row)
{
    gint cx, cy;
    gint i = 0;

    cy = row;
    i = row * canvas->config.width;

    for (cx = 0; cx < canvas->config.width; cx++, i++)
    {
        ChafaPixel block [CHAFA_SYMBOL_N_PIXELS];
        ChafaCanvasCell *cell = &canvas->cells [i];
        gunichar sym = 0;
        ChafaColorCandidates ccand;
        ChafaColor fg_col, bg_col;

        memset (cell, 0, sizeof (*cell));
        cell->c = ' ';

        fetch_canvas_pixel_block (canvas, cx, cy, block);

        if (canvas->config.symbol_map.n_symbols > 0)
        {
            if (canvas->work_factor_int >= 8)
                pick_symbol_and_colors_slow (canvas, block, &sym, &fg_col, &bg_col, NULL);
            else
                pick_symbol_and_colors_fast (canvas, block, &sym, &fg_col, &bg_col, NULL);

            cell->c = sym;

            if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_256)
            {
                chafa_pick_color_256 (&fg_col, canvas->config.color_space, &ccand);
                cell->fg_color = ccand.index [0];
                chafa_pick_color_256 (&bg_col, canvas->config.color_space, &ccand);
                cell->bg_color = ccand.index [0];
            }
            else if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_240)
            {
                chafa_pick_color_240 (&fg_col, canvas->config.color_space, &ccand);
                cell->fg_color = ccand.index [0];
                chafa_pick_color_240 (&bg_col, canvas->config.color_space, &ccand);
                cell->bg_color = ccand.index [0];
            }
            else if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_16)
            {
                chafa_pick_color_16 (&fg_col, canvas->config.color_space, &ccand);
                cell->fg_color = ccand.index [0];
                chafa_pick_color_16 (&bg_col, canvas->config.color_space, &ccand);
                cell->bg_color = ccand.index [0];
            }
            else if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG_BGFG)
            {
                chafa_pick_color_fgbg (&fg_col, canvas->config.color_space,
                                       &canvas->fg_color, &canvas->bg_color, &ccand);
                cell->fg_color = ccand.index [0];
                chafa_pick_color_fgbg (&bg_col, canvas->config.color_space,
                                       &canvas->fg_color, &canvas->bg_color, &ccand);
                cell->bg_color = ccand.index [0];
            }
            else
            {
                cell->fg_color = chafa_pack_color (&fg_col);
                cell->bg_color = chafa_pack_color (&bg_col);
            }
        }
#if 1
        /* If we produced a featureless cell, try fill */

        /* FIXME: Check popcount == 0 or == 64 instead of symbol char */
        if (sym == 0 || sym == ' ' || sym == 0x2588 || cell->fg_color == cell->bg_color)
        {
            apply_fill (canvas, block, cell);
        }
#endif
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
multiply_alpha (ChafaCanvas *canvas)
{
    ChafaPixel *p0, *p1;

    p0 = canvas->pixels;
    p1 = p0 + canvas->width_pixels * canvas->height_pixels;

    for ( ; p0 < p1; p0++)
    {
        chafa_color_mix (&p0->col, &canvas->bg_color, &p0->col, 1000 - ((p0->col.ch [3] * 1000) / 255));
    }
}

static gint
rgb_to_intensity_fast (const ChafaColor *color)
{
    /* Sum to 8x so we can divide by shifting later */
    return color->ch [0] * 3 + color->ch [1] * 4 + color->ch [2];
}

static void
sum_histograms (const Histogram *hist_in, Histogram *hist_accum)
{
    gint i;

    for (i = 0; i < INTENSITY_MAX; i++)
    {
        hist_accum->c [i] += hist_in->c [i];
    }
}

static void
histogram_calc_bounds (ChafaCanvas *canvas, Histogram *hist, gint crop_pct)
{
    gint64 pixels_crop;
    gint i;
    gint t;

    pixels_crop = (canvas->width_pixels * canvas->height_pixels * (((gint64) crop_pct * 1024) / 100)) / 1024;

    /* Find lower bound */

    for (i = 0, t = pixels_crop; i < INTENSITY_MAX; i++)
    {
        t -= hist->c [i];
        if (t <= 0)
            break;
    }

    hist->min = i;

    /* Find upper bound */

    for (i = INTENSITY_MAX - 1, t = pixels_crop; i >= 0; i--)
    {
        t -= hist->c [i];
        if (t <= 0)
            break;
    }

    hist->max = i;
}

static gint16
normalize_ch (gint16 v, gint min, gint factor)
{
    gint vt = v;

    vt -= min;
    if (vt < 0)
        vt = 0;
    vt *= factor;
    vt /= FIXED_MULT;
    if (vt > 255)
        vt = 255;
    return vt;
}

static void
normalize_rgb (ChafaCanvas *canvas, const Histogram *hist, gint dest_y, gint n_rows)
{
    ChafaPixel *p0, *p1;
    gint factor;

    /* Make sure range is more or less sane */

    if (hist->min == hist->max)
        return;

#if 0
    if (min > 512)
        min = 512;
    if (max < 1536)
        max = 1536;
#endif

    /* Adjust intensities */

    factor = ((INTENSITY_MAX - 1) * FIXED_MULT) / (hist->max - hist->min);

#if 0
    g_printerr ("[%d-%d] * %d, crop=%d     \n", min, max, factor, pixels_crop);
#endif

    p0 = canvas->pixels + dest_y * canvas->width_pixels;
    p1 = p0 + n_rows * canvas->width_pixels;

    for ( ; p0 < p1; p0++)
    {
        p0->col.ch [0] = normalize_ch (p0->col.ch [0], hist->min / 8, factor);
        p0->col.ch [1] = normalize_ch (p0->col.ch [1], hist->min / 8, factor);
        p0->col.ch [2] = normalize_ch (p0->col.ch [2], hist->min / 8, factor);
    }
}

static void
boost_saturation_rgb (ChafaColor *col)
{
#define Pr .299
#define Pg .587
#define Pb .144
    gdouble P = sqrt ((col->ch [0]) * (gdouble) (col->ch [0]) * Pr
                      + (col->ch [1]) * (gdouble) (col->ch [1]) * Pg
                      + (col->ch [2]) * (gdouble) (col->ch [2]) * Pb);

    col->ch [0] = (P + ((gdouble) col->ch [0] - P) * 2);
    col->ch [1] = (P + ((gdouble) col->ch [1] - P) * 2);
    col->ch [2] = (P + ((gdouble) col->ch [2] - P) * 2);
}

static void
clamp_color_rgb (ChafaColor *col)
{
    col->ch [0] = CLAMP (col->ch [0], 0, 255);
    col->ch [1] = CLAMP (col->ch [1], 0, 255);
    col->ch [2] = CLAMP (col->ch [2], 0, 255);
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
bayer_dither_pixel (ChafaCanvas *canvas, ChafaPixel *pixel, gint *matrix, gint x, gint y, guint size_shift, guint size_mask)
{
    gint bayer_index = (((y >> canvas->dither_grain_height_shift) & size_mask) << size_shift)
        + ((x >> canvas->dither_grain_width_shift) & size_mask);
    gint bayer_mod = matrix [bayer_index];
    gint i;

    for (i = 0; i < 4; i++)
    {
        pixel->col.ch [i] += bayer_mod;
        if (pixel->col.ch [i] < 0)
            pixel->col.ch [i] = 0;
        if (pixel->col.ch [i] > 255)
            pixel->col.ch [i] = 255;
    }
}

/* pixel must point to top-left pixel of the grain to be dithered */
static void
fs_dither_grain (ChafaCanvas *canvas, ChafaPixel *pixel, const ChafaPixel *error_in,
                 ChafaPixel *error_out_0, ChafaPixel *error_out_1,
                 ChafaPixel *error_out_2, ChafaPixel *error_out_3)
{
    gint grain_shift = canvas->dither_grain_width_shift + canvas->dither_grain_height_shift;
    ChafaPixel next_error = { 0 };
    ChafaPixel accum = { 0 };
    ChafaColorCandidates cand = { 0 };
    ChafaPixel *p;
    const ChafaColor *c;
    gint x, y, i;

    p = pixel;

    for (y = 0; y < canvas->config.dither_grain_height; y++)
    {
        for (x = 0; x < canvas->config.dither_grain_width; x++, p++)
        {
            for (i = 0; i < 3; i++)
            {
                p->col.ch [i] += error_in->col.ch [i];

                if (canvas->config.color_space == CHAFA_COLOR_SPACE_RGB)
                {
                    if (p->col.ch [i] < 0)
                    {
                        next_error.col.ch [i] += p->col.ch [i];
                        p->col.ch [i] = 0;
                    }
                    else if (p->col.ch [i] > 255)
                    {
                        next_error.col.ch [i] += p->col.ch [i] - 255;
                        p->col.ch [i] = 255;
                    }
                }

                accum.col.ch [i] += p->col.ch [i];
            }
        }

        p += canvas->width_pixels - canvas->config.dither_grain_width;
    }

    for (i = 0; i < 3; i++)
        accum.col.ch [i] >>= grain_shift;

    /* Don't try to dither alpha */
    accum.col.ch [3] = 0xff;

    if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_256)
        chafa_pick_color_256 (&accum.col, canvas->config.color_space, &cand);
    else if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_240)
        chafa_pick_color_240 (&accum.col, canvas->config.color_space, &cand);
    else if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_16)
        chafa_pick_color_16 (&accum.col, canvas->config.color_space, &cand);
    else
        chafa_pick_color_fgbg (&accum.col, canvas->config.color_space,
                               &canvas->fg_color, &canvas->bg_color, &cand);

    c = chafa_get_palette_color_256 (cand.index [0], canvas->config.color_space);

    for (i = 0; i < 3; i++)
    {
        /* FIXME: Floating point op is slow. Factor this out and make
         * dither_intensity == 1.0 the fast path. */
        next_error.col.ch [i] = ((next_error.col.ch [i] >> grain_shift) + (accum.col.ch [i] - c->ch [i]) * canvas->config.dither_intensity);

        error_out_0->col.ch [i] += next_error.col.ch [i] * 7 / 16;
        error_out_1->col.ch [i] += next_error.col.ch [i] * 1 / 16;
        error_out_2->col.ch [i] += next_error.col.ch [i] * 5 / 16;
        error_out_3->col.ch [i] += next_error.col.ch [i] * 3 / 16;
    }
}

static void
convert_rgb_to_din99d (ChafaCanvas *canvas, gint dest_y, gint n_rows)
{
    ChafaPixel *pixel = canvas->pixels + dest_y * canvas->width_pixels;
    ChafaPixel *pixel_max = pixel + n_rows * canvas->width_pixels;

    /* RGB -> DIN99d */

    for ( ; pixel < pixel_max; pixel++)
    {
        chafa_color_rgb_to_din99d (&pixel->col, &pixel->col);
    }
}

static void
bayer_dither (ChafaCanvas *canvas, gint dest_y, gint n_rows)
{
    ChafaPixel *pixel = canvas->pixels + dest_y * canvas->width_pixels;
    ChafaPixel *pixel_max = pixel + n_rows * canvas->width_pixels;
    guint bayer_size_mask = (1 << canvas->bayer_size_shift) - 1;
    gint x, y;

    for (y = dest_y; pixel < pixel_max; y++)
    {
        for (x = 0; x < canvas->width_pixels; x++)
        {
            bayer_dither_pixel (canvas, pixel, canvas->bayer_matrix, x, y,
                                canvas->bayer_size_shift, bayer_size_mask);
            pixel++;
        }
    }
}

static void
fs_dither (ChafaCanvas *canvas, gint dest_y, gint n_rows)
{
    ChafaPixel *pixel;
    ChafaPixel *error_rows;
    ChafaPixel *error_row [2];
    ChafaPixel *pp;
    gint width_grains = canvas->width_pixels >> canvas->dither_grain_width_shift;
    gint x, y;

    g_assert (canvas->width_pixels % canvas->config.dither_grain_width == 0);
    g_assert (dest_y % canvas->config.dither_grain_height == 0);
    g_assert (n_rows % canvas->config.dither_grain_height == 0);

    dest_y >>= canvas->dither_grain_height_shift;
    n_rows >>= canvas->dither_grain_height_shift;

    error_rows = alloca (width_grains * 2 * sizeof (ChafaPixel));
    error_row [0] = error_rows;
    error_row [1] = error_rows + width_grains;

    memset (error_row [0], 0, width_grains * sizeof (ChafaPixel));

    for (y = dest_y; y < dest_y + n_rows; y++)
    {
        memset (error_row [1], 0, width_grains * sizeof (ChafaPixel));

        if (!(y & 1))
        {
            /* Forwards pass */
            pixel = canvas->pixels + (y << canvas->dither_grain_height_shift) * canvas->width_pixels;

            fs_dither_grain (canvas, pixel, error_row [0],
                             error_row [0] + 1,
                             error_row [1] + 1,
                             error_row [1],
                             error_row [1] + 1);
            pixel += canvas->config.dither_grain_width;

            for (x = 1; ((x + 1) << canvas->dither_grain_width_shift) < canvas->width_pixels; x++)
            {
                fs_dither_grain (canvas, pixel, error_row [0] + x,
                                 error_row [0] + x + 1,
                                 error_row [1] + x + 1,
                                 error_row [1] + x,
                                 error_row [1] + x - 1);
                pixel += canvas->config.dither_grain_width;
            }

            fs_dither_grain (canvas, pixel, error_row [0] + x,
                             error_row [1] + x,
                             error_row [1] + x,
                             error_row [1] + x - 1,
                             error_row [1] + x - 1);
        }
        else
        {
            /* Backwards pass */
            pixel = canvas->pixels + (y << canvas->dither_grain_height_shift) * canvas->width_pixels + canvas->width_pixels - canvas->config.dither_grain_width;

            fs_dither_grain (canvas, pixel, error_row [0] + width_grains - 1,
                             error_row [0] + width_grains - 2,
                             error_row [1] + width_grains - 2,
                             error_row [1] + width_grains - 1,
                             error_row [1] + width_grains - 2);

            pixel -= canvas->config.dither_grain_width;

            for (x = width_grains - 2; x > 0; x--)
            {
                fs_dither_grain (canvas, pixel, error_row [0] + x,
                                 error_row [0] + x - 1,
                                 error_row [1] + x - 1,
                                 error_row [1] + x,
                                 error_row [1] + x + 1);
                pixel -= canvas->config.dither_grain_width;
            }

            fs_dither_grain (canvas, pixel, error_row [0],
                             error_row [1],
                             error_row [1],
                             error_row [1] + 1,
                             error_row [1] + 1);
        }

        pp = error_row [0];
        error_row [0] = error_row [1];
        error_row [1] = pp;
    }
}

static void
bayer_and_convert_rgb_to_din99d (ChafaCanvas *canvas, gint dest_y, gint n_rows)
{
    ChafaPixel *pixel = canvas->pixels + dest_y * canvas->width_pixels;
    ChafaPixel *pixel_max = pixel + n_rows * canvas->width_pixels;
    guint bayer_size_mask = (1 << canvas->bayer_size_shift) - 1;
    gint x, y;

    for (y = dest_y; pixel < pixel_max; y++)
    {
        for (x = 0; x < canvas->width_pixels; x++)
        {
            bayer_dither_pixel (canvas, pixel, canvas->bayer_matrix, x, y,
                                canvas->bayer_size_shift, bayer_size_mask);
            chafa_color_rgb_to_din99d (&pixel->col, &pixel->col);
            pixel++;
        }
    }
}

static void
fs_and_convert_rgb_to_din99d (ChafaCanvas *canvas, gint dest_y, gint n_rows)
{
    convert_rgb_to_din99d (canvas, dest_y, n_rows);
    fs_dither (canvas, dest_y, n_rows);
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

typedef struct
{
    ChafaCanvas *canvas;
    Histogram hist;
    gint n_batches_pixels;
    gint n_rows_per_batch_pixels;
    gint n_batches_cells;
    gint n_rows_per_batch_cells;
}
PrepareContext;

typedef struct
{
    gint first_row;
    gint n_rows;
    Histogram hist;
}
PreparePixelsBatch1;

static void
prepare_pixels_1_worker (PreparePixelsBatch1 *work, PrepareContext *prep_ctx)
{
    ChafaPixel *pixel;
    ChafaCanvas *canvas;
    gint dest_y;
    gint px, py;
    gint x_inc, y_inc;
    gint alpha_sum = 0;
    const guint8 *data;
    gint n_rows;
    gint rowstride;

    canvas = prep_ctx->canvas;
    dest_y = work->first_row;
    data = canvas->src_pixels;
    n_rows = work->n_rows;
    rowstride = canvas->src_rowstride;

    x_inc = (canvas->src_width * FIXED_MULT) / (canvas->width_pixels);
    y_inc = (canvas->src_height * FIXED_MULT) / (canvas->height_pixels);

    pixel = canvas->pixels + dest_y * canvas->width_pixels;

    for (py = dest_y; py < dest_y + n_rows; py++)
    {
        const guint8 *data_row_p;

        data_row_p = data + ((py * y_inc) / FIXED_MULT) * rowstride;

        for (px = 0; px < canvas->width_pixels; px++)
        {
            const guint8 *data_p = data_row_p + ((px * x_inc) / FIXED_MULT) * 4;
            ChafaColor col;
            gint v;

            col.ch [0] = *data_p++;
            col.ch [1] = *data_p++;
            col.ch [2] = *data_p++;
            col.ch [3] = *data_p;

            alpha_sum += (0xff - col.ch [3]);

            if (canvas->config.preprocessing_enabled
                && canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_16)
            {
                boost_saturation_rgb (&col);
                clamp_color_rgb (&col);
            }

            pixel->col = col;

            /* Build histogram */
            v = rgb_to_intensity_fast (&pixel->col);
            work->hist.c [v]++;

            pixel++;
        }
    }

    if (alpha_sum > 0)
        g_atomic_int_set (&canvas->have_alpha_int, 1);
}

static void
prepare_pixels_pass_1 (PrepareContext *prep_ctx)
{
    GThreadPool *thread_pool;
    PreparePixelsBatch1 *batches;
    gint cy;
    gint i;

    /* First pass
     * ----------
     *
     * - Scale and convert pixel format
     * - Apply local preprocessing like saturation boost (optional)
     * - Generate histogram for later passes (e.g. for normalization)
     */

    batches = g_new0 (PreparePixelsBatch1, prep_ctx->n_batches_pixels);

    thread_pool = g_thread_pool_new ((GFunc) prepare_pixels_1_worker,
                                     prep_ctx,
                                     g_get_num_processors (),
                                     FALSE,
                                     NULL);

    for (cy = 0, i = 0;
         cy < prep_ctx->canvas->height_pixels;
         cy += prep_ctx->n_rows_per_batch_pixels, i++)
    {
        PreparePixelsBatch1 *batch = &batches [i];

        batch->first_row = cy;
        batch->n_rows = MIN (prep_ctx->canvas->height_pixels - cy, prep_ctx->n_rows_per_batch_pixels);

        g_thread_pool_push (thread_pool, batch, NULL);
    }

    /* Wait for threads to finish */
    g_thread_pool_free (thread_pool, FALSE, TRUE);

    /* Generate final histogram */
    if (prep_ctx->canvas->config.preprocessing_enabled)
    {
        for (i = 0; i < prep_ctx->n_batches_pixels; i++)
            sum_histograms (&batches [i].hist, &prep_ctx->hist);

        histogram_calc_bounds (prep_ctx->canvas, &prep_ctx->hist,
                               prep_ctx->canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_16 ? INDEXED_16_CROP_PCT : INDEXED_2_CROP_PCT);
    }

    g_free (batches);
}

typedef struct
{
    gint first_row;
    gint n_rows;
}
PreparePixelsBatch2;

static void
prepare_pixels_2_worker (PreparePixelsBatch2 *work, PrepareContext *prep_ctx)
{
    ChafaCanvas *canvas = prep_ctx->canvas;

    if (canvas->config.preprocessing_enabled
        && (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_16
            || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG_BGFG
            || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG))
        normalize_rgb (canvas, &prep_ctx->hist, work->first_row, work->n_rows);

    if (canvas->config.color_space == CHAFA_COLOR_SPACE_DIN99D)
    {
        if (canvas->config.dither_mode == CHAFA_DITHER_MODE_ORDERED)
        {
            bayer_and_convert_rgb_to_din99d (canvas,
                                             work->first_row,
                                             work->n_rows);
        }
        else if (canvas->config.dither_mode == CHAFA_DITHER_MODE_DIFFUSION)
        {
            fs_and_convert_rgb_to_din99d (canvas,
                                          work->first_row,
                                          work->n_rows);
        }
        else
        {
            convert_rgb_to_din99d (canvas,
                                   work->first_row,
                                   work->n_rows);
        }
    }
    else if (canvas->config.dither_mode == CHAFA_DITHER_MODE_ORDERED)
    {
        bayer_dither (canvas,
                      work->first_row,
                      work->n_rows);
    }
    else if (canvas->config.dither_mode == CHAFA_DITHER_MODE_DIFFUSION)
    {
        fs_dither (canvas,
                   work->first_row,
                   work->n_rows);
    }
}

static void
prepare_pixels_pass_2 (PrepareContext *prep_ctx)
{
    GThreadPool *thread_pool;
    PreparePixelsBatch1 *batches;
    gint cy;
    gint i;

    /* Second pass
     * -----------
     *
     * - Normalization (optional)
     * - Dithering (optional)
     * - Color space conversion; DIN99d (optional)
     */

    batches = g_new0 (PreparePixelsBatch1, prep_ctx->n_batches_pixels);

    thread_pool = g_thread_pool_new ((GFunc) prepare_pixels_2_worker,
                                     prep_ctx,
                                     g_get_num_processors (),
                                     FALSE,
                                     NULL);

    for (cy = 0, i = 0;
         cy < prep_ctx->canvas->height_pixels;
         cy += prep_ctx->n_rows_per_batch_pixels, i++)
    {
        PreparePixelsBatch1 *batch = &batches [i];

        batch->first_row = cy;
        batch->n_rows = MIN (prep_ctx->canvas->height_pixels - cy, prep_ctx->n_rows_per_batch_pixels);

        g_thread_pool_push (thread_pool, batch, NULL);
    }

    /* Wait for threads to finish */
    g_thread_pool_free (thread_pool, FALSE, TRUE);

    g_free (batches);
}

static void
prepare_pixel_data (ChafaCanvas *canvas)
{
    PrepareContext prep_ctx = { 0 };
    guint n_cpus;

    n_cpus = g_get_num_processors ();

    prep_ctx.canvas = canvas;
    prep_ctx.n_batches_pixels = (canvas->height_pixels + n_cpus - 1) / n_cpus;
    prep_ctx.n_rows_per_batch_pixels = (canvas->height_pixels + prep_ctx.n_batches_pixels - 1) / prep_ctx.n_batches_pixels;
    prep_ctx.n_batches_cells = (canvas->config.height + n_cpus - 1) / n_cpus;
    prep_ctx.n_rows_per_batch_cells = (canvas->config.height + prep_ctx.n_batches_cells - 1) / prep_ctx.n_batches_cells;

    prepare_pixels_pass_1 (&prep_ctx);
    prepare_pixels_pass_2 (&prep_ctx);
}

static void
emit_ansi_truecolor (ChafaCanvas *canvas, GString *gs, gint i, gint i_max)
{
    for ( ; i < i_max; i++)
    {
        ChafaCanvasCell *cell = &canvas->cells [i];
        ChafaColor fg, bg;

        chafa_unpack_color (cell->fg_color, &fg);
        chafa_unpack_color (cell->bg_color, &bg);

        if (fg.ch [3] < canvas->config.alpha_threshold)
        {
            if (bg.ch [3] < canvas->config.alpha_threshold)
            {
                /* FIXME: Respect include/exclude for space */
                g_string_append (gs, "\x1b[0m ");
            }
            else
            {
                g_string_append_printf (gs, "\x1b[0m\x1b[7m\x1b[38;2;%d;%d;%dm",
                                        bg.ch [0], bg.ch [1], bg.ch [2]);
                g_string_append_unichar (gs, cell->c);
            }
        }
        else if (bg.ch [3] < canvas->config.alpha_threshold)
        {
            g_string_append_printf (gs, "\x1b[0m\x1b[38;2;%d;%d;%dm",
                                    fg.ch [0], fg.ch [1], fg.ch [2]);
            g_string_append_unichar (gs, cell->c);
        }
        else
        {
            g_string_append_printf (gs, "\x1b[0m\x1b[38;2;%d;%d;%dm\x1b[48;2;%d;%d;%dm",
                                    fg.ch [0], fg.ch [1], fg.ch [2],
                                    bg.ch [0], bg.ch [1], bg.ch [2]);
            g_string_append_unichar (gs, cell->c);
        }
    }
}

static void
emit_ansi_256 (ChafaCanvas *canvas, GString *gs, gint i, gint i_max)
{
    for ( ; i < i_max; i++)
    {
        ChafaCanvasCell *cell = &canvas->cells [i];

        if (cell->fg_color == CHAFA_PALETTE_INDEX_TRANSPARENT)
        {
            if (cell->bg_color == CHAFA_PALETTE_INDEX_TRANSPARENT)
            {
                g_string_append (gs, "\x1b[0m ");
            }
            else
            {
                g_string_append_printf (gs, "\x1b[0m\x1b[7m\x1b[38;5;%dm",
                                        cell->bg_color);
                g_string_append_unichar (gs, cell->c);
            }
        }
        else if (cell->bg_color == CHAFA_PALETTE_INDEX_TRANSPARENT)
        {
            g_string_append_printf (gs, "\x1b[0m\x1b[38;5;%dm",
                                    cell->fg_color);
            g_string_append_unichar (gs, cell->c);
        }
        else
        {
            g_string_append_printf (gs, "\x1b[0m\x1b[38;5;%dm\x1b[48;5;%dm",
                                    cell->fg_color,
                                    cell->bg_color);
            g_string_append_unichar (gs, cell->c);
        }
    }
}

/* Uses aixterm control codes for bright colors */
static void
emit_ansi_16 (ChafaCanvas *canvas, GString *gs, gint i, gint i_max)
{
    for ( ; i < i_max; i++)
    {
        ChafaCanvasCell *cell = &canvas->cells [i];

        if (cell->fg_color == CHAFA_PALETTE_INDEX_TRANSPARENT)
        {
            if (cell->bg_color == CHAFA_PALETTE_INDEX_TRANSPARENT)
            {
                g_string_append (gs, "\x1b[0m ");
            }
            else
            {
                g_string_append_printf (gs, "\x1b[0m\x1b[7m\x1b[%dm",
                                        cell->bg_color < 8 ? cell->bg_color + 30
                                                           : cell->bg_color + 90 - 8);
                g_string_append_unichar (gs, cell->c);
            }
        }
        else if (cell->bg_color == CHAFA_PALETTE_INDEX_TRANSPARENT)
        {
            g_string_append_printf (gs, "\x1b[0m\x1b[%dm",
                                    cell->fg_color < 8 ? cell->fg_color + 30
                                                       : cell->fg_color + 90 - 8);
            g_string_append_unichar (gs, cell->c);
        }
        else
        {
            g_string_append_printf (gs, "\x1b[0m\x1b[%dm\x1b[%dm",
                                    cell->fg_color < 8 ? cell->fg_color + 30
                                                       : cell->fg_color + 90 - 8,
                                    cell->bg_color < 8 ? cell->bg_color + 40
                                                       : cell->bg_color + 100 - 8);
            g_string_append_unichar (gs, cell->c);
        }
    }
}

static void
emit_ansi_fgbg_bgfg (ChafaCanvas *canvas, GString *gs, gint i, gint i_max)
{
    gunichar blank_symbol = 0;

    if (chafa_symbol_map_has_symbol (&canvas->config.symbol_map, ' '))
    {
        blank_symbol = ' ';
    }
    else if (chafa_symbol_map_has_symbol (&canvas->config.symbol_map, 0x2588 /* Solid block */ ))
    {
        blank_symbol = 0x2588;
    }

    for ( ; i < i_max; i++)
    {
        ChafaCanvasCell *cell = &canvas->cells [i];
        gboolean invert = FALSE;
        gunichar c = cell->c;

        if (cell->fg_color == cell->bg_color && blank_symbol != 0)
        {
            c = blank_symbol;
            if (blank_symbol == 0x2588)
                invert = TRUE;
        }

        if (cell->bg_color == CHAFA_PALETTE_INDEX_FG)
        {
            invert ^= TRUE;
        }

        g_string_append_printf (gs, "\x1b[%dm",
                                invert ? 7 : 0);
        g_string_append_unichar (gs, c);
    }
}

static void
emit_ansi_fgbg (ChafaCanvas *canvas, GString *gs, gint i, gint i_max)
{
    for ( ; i < i_max; i++)
    {
        ChafaCanvasCell *cell = &canvas->cells [i];

        g_string_append_unichar (gs, cell->c);
    }
}

static GString *
build_ansi_gstring (ChafaCanvas *canvas)
{
    GString *gs = g_string_new ("");
    gint i, i_max, i_step, i_next;

    maybe_clear (canvas);

    i = 0;
    i_max = canvas->config.width * canvas->config.height;
    i_step = canvas->config.width;

    for ( ; i < i_max; i = i_next)
    {
        i_next = i + i_step;

        switch (canvas->config.canvas_mode)
        {
            case CHAFA_CANVAS_MODE_TRUECOLOR:
                emit_ansi_truecolor (canvas, gs, i, i_next);
                break;
            case CHAFA_CANVAS_MODE_INDEXED_256:
            case CHAFA_CANVAS_MODE_INDEXED_240:
                emit_ansi_256 (canvas, gs, i, i_next);
                break;
            case CHAFA_CANVAS_MODE_INDEXED_16:
                emit_ansi_16 (canvas, gs, i, i_next);
                break;
            case CHAFA_CANVAS_MODE_FGBG_BGFG:
                emit_ansi_fgbg_bgfg (canvas, gs, i, i_next);
                break;
            case CHAFA_CANVAS_MODE_FGBG:
                emit_ansi_fgbg (canvas, gs, i, i_next);
                break;
            case CHAFA_CANVAS_MODE_MAX:
                g_assert_not_reached ();
                break;
        }

        /* No control codes in FGBG mode */
        if (canvas->config.canvas_mode != CHAFA_CANVAS_MODE_FGBG)
            g_string_append (gs, "\x1b[0m");

        /* Last line should not end in newline */
        if (i_next < i_max)
            g_string_append_c (gs, '\n');
    }

    return gs;
}

static gint
calc_dither_grain_shift (gint size)
{
    switch (size)
    {
        case 1:
            return 0;
        case 2:
            return 1;
        case 4:
            return 2;
        case 8:
            return 3;
        default:
            g_assert_not_reached ();
    }

    return 0;
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
    canvas->width_pixels = canvas->config.width * CHAFA_SYMBOL_WIDTH_PIXELS;
    canvas->height_pixels = canvas->config.height * CHAFA_SYMBOL_HEIGHT_PIXELS;
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
     * There is also no reason to dither in truecolor mode. */
    if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_TRUECOLOR)
    {
        canvas->config.color_space = CHAFA_COLOR_SPACE_RGB;
        canvas->config.dither_mode = CHAFA_DITHER_MODE_NONE;
    }

    canvas->dither_grain_width_shift = calc_dither_grain_shift (canvas->config.dither_grain_width);
    canvas->dither_grain_height_shift = calc_dither_grain_shift (canvas->config.dither_grain_height);

    if (canvas->config.dither_mode == CHAFA_DITHER_MODE_ORDERED)
    {
        gdouble dither_intensity;

        switch (canvas->config.canvas_mode)
        {
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

        canvas->bayer_size_shift = BAYER_MATRIX_DIM_SHIFT;
        canvas->bayer_matrix = chafa_gen_bayer_matrix (BAYER_MATRIX_DIM, dither_intensity * canvas->config.dither_intensity);
    }

    update_display_colors (canvas);

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
    canvas->bayer_matrix = g_memdup (orig->bayer_matrix, BAYER_MATRIX_SIZE * sizeof (gint));

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
        g_free (canvas->pixels);
        g_free (canvas->cells);
        g_free (canvas->bayer_matrix);
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
 **/
void
chafa_canvas_set_contents_rgba8 (ChafaCanvas *canvas, const guint8 *src_pixels,
                                 gint src_width, gint src_height, gint src_rowstride)
{
    g_return_if_fail (canvas != NULL);
    g_return_if_fail (canvas->refs > 0);
    g_return_if_fail (src_pixels != NULL);
    g_return_if_fail (src_width >= 0);
    g_return_if_fail (src_height >= 0);

    if (src_width == 0 || src_height == 0)
        return;

    canvas->pixels = g_new (ChafaPixel, canvas->width_pixels * canvas->height_pixels);
    canvas->hist = g_new (Histogram, 1);

    canvas->src_pixels = src_pixels;
    canvas->src_width = src_width;
    canvas->src_height = src_height;
    canvas->src_rowstride = src_rowstride;
    canvas->have_alpha_int = 0;

    prepare_pixel_data (canvas);

    if (canvas->have_alpha_int)
        canvas->have_alpha = TRUE;

    if (canvas->have_alpha)
        multiply_alpha (canvas);

    if (canvas->config.alpha_threshold == 0)
        canvas->have_alpha = FALSE;

    update_cells (canvas);
    canvas->needs_clear = FALSE;

    g_free (canvas->hist);
    g_free (canvas->pixels);
    canvas->pixels = NULL;
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
    g_return_val_if_fail (canvas != NULL, NULL);
    g_return_val_if_fail (canvas->refs > 0, NULL);

    return build_ansi_gstring (canvas);
}
