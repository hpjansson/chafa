/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2022 Hans Petter Jansson
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
#include "internal/chafa-batch.h"
#include "internal/chafa-canvas-internal.h"
#include "internal/chafa-canvas-printer.h"
#include "internal/chafa-private.h"
#include "internal/chafa-pixops.h"
#include "internal/chafa-work-cell.h"
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
#define DITHER_BASE_INTENSITY_8C   0.5
#define DITHER_BASE_INTENSITY_16C  0.25
#define DITHER_BASE_INTENSITY_256C 0.1

typedef struct
{
    ChafaColorPair colors;
    gint error;
}
SymbolEval;

typedef struct
{
    ChafaColorPair colors;
    gint error [2];
}
SymbolEval2;

static ChafaColor
threshold_alpha (ChafaColor col, gint alpha_threshold)
{
    col.ch [3] = col.ch [3] < alpha_threshold ? 0 : 255;
    return col;
}

static gint
color_to_rgb (const ChafaCanvas *canvas, ChafaColor col)
{
    col = threshold_alpha (col, canvas->config.alpha_threshold);
    if (col.ch [3] == 0)
        return -1;

    return ((gint) col.ch [0] << 16) | ((gint) col.ch [1] << 8) | (gint) col.ch [2];
}

static gint
packed_rgba_to_rgb (const ChafaCanvas *canvas, guint32 rgba)
{
    ChafaColor col;

    chafa_unpack_color (rgba, &col);
    return color_to_rgb (canvas, col);
}

static ChafaColor
packed_rgb_to_color (gint rgb)
{
    ChafaColor col;

    if (rgb < 0)
    {
        col.ch [0] = 0x80;
        col.ch [1] = 0x80;
        col.ch [2] = 0x80;
        col.ch [3] = 0x00;
    }
    else
    {
        col.ch [0] = (rgb >> 16) & 0xff;
        col.ch [1] = (rgb >> 8) & 0xff;
        col.ch [2] = rgb & 0xff;
        col.ch [3] = 0xff;
    }

    return col;
}

static guint32
packed_rgb_to_rgba (gint rgb)
{
    ChafaColor col;

    col = packed_rgb_to_color (rgb);
    return chafa_pack_color (&col);
}

static gint16
packed_rgb_to_index (const ChafaPalette *palette, ChafaColorSpace cs, gint rgb)
{
    ChafaColorCandidates ccand;
    ChafaColor col;

    if (rgb < 0)
        return CHAFA_PALETTE_INDEX_TRANSPARENT;

    col = packed_rgb_to_color (rgb);
    chafa_palette_lookup_nearest (palette, cs, &col, &ccand);
    return ccand.index [0];
}

static guint32
transparent_cell_color (ChafaCanvasMode canvas_mode)
{
    if (canvas_mode == CHAFA_CANVAS_MODE_TRUECOLOR)
    {
        const ChafaColor col = { { 0x80, 0x80, 0x80, 0x00 } };
        return chafa_pack_color (&col);
    }
    else
    {
        return CHAFA_PALETTE_INDEX_TRANSPARENT;
    }
}

static void
eval_symbol_colors (ChafaCanvas *canvas, ChafaWorkCell *wcell, const ChafaSymbol *sym, SymbolEval *eval)
{
    if (canvas->config.color_extractor == CHAFA_COLOR_EXTRACTOR_AVERAGE)
        chafa_work_cell_get_mean_colors_for_symbol (wcell, sym, &eval->colors);
    else
        chafa_work_cell_get_median_colors_for_symbol (wcell, sym, &eval->colors);
}

static void
eval_symbol_colors_wide (ChafaCanvas *canvas, ChafaWorkCell *wcell_a, ChafaWorkCell *wcell_b,
                         const ChafaSymbol *sym_a, const ChafaSymbol *sym_b,
                         SymbolEval2 *eval)
{
    SymbolEval part_eval [2];

    eval_symbol_colors (canvas, wcell_a, sym_a, &part_eval [0]);
    eval_symbol_colors (canvas, wcell_b, sym_b, &part_eval [1]);

    eval->colors.colors [CHAFA_COLOR_PAIR_FG]
        = chafa_color_average_2 (part_eval [0].colors.colors [CHAFA_COLOR_PAIR_FG],
                                 part_eval [1].colors.colors [CHAFA_COLOR_PAIR_FG]);
    eval->colors.colors [CHAFA_COLOR_PAIR_BG]
        = chafa_color_average_2 (part_eval [0].colors.colors [CHAFA_COLOR_PAIR_BG],
                                 part_eval [1].colors.colors [CHAFA_COLOR_PAIR_BG]);
}

static gint
calc_error_plain (const ChafaPixel *block, const ChafaColorPair *color_pair, const guint8 *cov)
{
    gint error = 0;
    gint i;

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        guint8 p = *cov++;
        const ChafaPixel *p0 = block++;

        error += chafa_color_diff_fast (&color_pair->colors [p], &p0->col);
    }

    return error;
}

static void
eval_symbol_error (const ChafaWorkCell *wcell,
                   const ChafaSymbol *sym, SymbolEval *eval,
                   const ChafaPalette *fg_palette,
                   const ChafaPalette *bg_palette,
                   ChafaColorSpace color_space)
{
    const guint8 *covp = (guint8 *) &sym->coverage [0];
    ChafaColorPair pair;
    gint error;

    if (!bg_palette)
        bg_palette = fg_palette;
    if (!fg_palette)
        fg_palette = bg_palette;

    if (fg_palette)
    {
        pair.colors [CHAFA_COLOR_PAIR_FG] =
            *chafa_palette_get_color (
                fg_palette,
                color_space,
                chafa_palette_lookup_nearest (fg_palette, color_space,
                                              &eval->colors.colors [CHAFA_COLOR_PAIR_FG], NULL));
        pair.colors [CHAFA_COLOR_PAIR_BG] =
            *chafa_palette_get_color (
                bg_palette,
                color_space,
                chafa_palette_lookup_nearest (bg_palette, color_space,
                                              &eval->colors.colors [CHAFA_COLOR_PAIR_BG], NULL));
    }
    else
    {
        pair = eval->colors;
    }

#ifdef HAVE_SSE41_INTRINSICS
    if (chafa_have_sse41 ())
        error = calc_error_sse41 (wcell->pixels, &pair, covp);
    else
#endif
        error = calc_error_plain (wcell->pixels, &pair, covp);

    eval->error = error;
}

static void
eval_symbol_error_wide (const ChafaWorkCell *wcell_a, const ChafaWorkCell *wcell_b,
                        const ChafaSymbol2 *sym, SymbolEval2 *wide_eval,
                        const ChafaPalette *fg_palette,
                        const ChafaPalette *bg_palette,
                        ChafaColorSpace color_space)
{
    SymbolEval eval [2];

    eval [0].colors = wide_eval->colors;
    eval [1].colors = wide_eval->colors;

    eval_symbol_error (wcell_a, &sym->sym [0], &eval [0],
                       fg_palette, bg_palette, color_space);
    eval_symbol_error (wcell_b, &sym->sym [1], &eval [1],
                       fg_palette, bg_palette, color_space);

    wide_eval->error [0] = eval [0].error;
    wide_eval->error [1] = eval [1].error;
}

static void
eval_symbol (ChafaCanvas *canvas, ChafaWorkCell *wcell, gint sym_index,
             gint *best_sym_index_out, SymbolEval *best_eval_inout)
{
    const ChafaSymbol *sym;
    SymbolEval eval;

    sym = &canvas->config.symbol_map.symbols [sym_index];

    if (canvas->config.fg_only_enabled)
    {
        eval.colors = canvas->default_colors;
    }
    else
    {
        eval_symbol_colors (canvas, wcell, sym, &eval);
    }

    if (canvas->use_quantized_error)
    {
        eval_symbol_error (wcell, sym, &eval, &canvas->fg_palette,
                           &canvas->bg_palette, canvas->config.color_space);
    }
    else
    {
        eval_symbol_error (wcell, sym, &eval, NULL, NULL,
                           canvas->config.color_space);
    }

    if (eval.error < best_eval_inout->error)
    {
        *best_sym_index_out = sym_index;
        *best_eval_inout = eval;
    }
}

static void
eval_symbol_wide (ChafaCanvas *canvas, ChafaWorkCell *wcell_a, ChafaWorkCell *wcell_b,
                  gint sym_index, gint *best_sym_index_out, SymbolEval2 *best_eval_inout)
{
    const ChafaSymbol2 *sym2;
    SymbolEval2 eval;

    sym2 = &canvas->config.symbol_map.symbols2 [sym_index];

    if (canvas->config.fg_only_enabled)
    {
        eval.colors = canvas->default_colors;
    }
    else
    {
        eval_symbol_colors_wide (canvas, wcell_a, wcell_b,
                                 &sym2->sym [0],
                                 &sym2->sym [1],
                                 &eval);
    }

    if (canvas->use_quantized_error)
    {
        eval_symbol_error_wide (wcell_a, wcell_b,
                                sym2,
                                &eval,
                                &canvas->fg_palette,
                                &canvas->bg_palette,
                                canvas->config.color_space);
    }
    else
    {
        eval_symbol_error_wide (wcell_a, wcell_b,
                                sym2,
                                &eval,
                                NULL,
                                NULL,
                                canvas->config.color_space);
    }

    if (eval.error [0] + eval.error [1] < best_eval_inout->error [0] + best_eval_inout->error [1])
    {
        *best_sym_index_out = sym_index;
        *best_eval_inout = eval;
    }
}

static void
pick_symbol_and_colors_slow (ChafaCanvas *canvas,
                             ChafaWorkCell *wcell,
                             gunichar *sym_out,
                             ChafaColorPair *color_pair_out,
                             gint *error_out)
{
    SymbolEval best_eval;
    gint best_symbol = -1;
    gint i;

    /* Find best symbol. All symbols are candidates. */

    best_eval.error = SYMBOL_ERROR_MAX;

    for (i = 0; canvas->config.symbol_map.symbols [i].c != 0; i++)
        eval_symbol (canvas, wcell, i, &best_symbol, &best_eval);

    chafa_leave_mmx ();  /* Make FPU happy again */

    /* Output */

    g_assert (best_symbol >= 0);

    if (canvas->extract_colors && canvas->config.fg_only_enabled)
        eval_symbol_colors (canvas, wcell, &canvas->config.symbol_map.symbols [best_symbol], &best_eval);

    *sym_out = canvas->config.symbol_map.symbols [best_symbol].c;
    *color_pair_out = best_eval.colors;

    if (error_out)
        *error_out = best_eval.error;
}

static void
pick_symbol_and_colors_wide_slow (ChafaCanvas *canvas,
                                  ChafaWorkCell *wcell_a,
                                  ChafaWorkCell *wcell_b,
                                  gunichar *sym_out,
                                  ChafaColorPair *color_pair_out,
                                  gint *error_a_out,
                                  gint *error_b_out)
{
    SymbolEval2 best_eval;
    gint best_symbol = -1;
    gint i;

    /* Find best symbol. All symbols are candidates. */

    best_eval.error [0] = best_eval.error [1] = SYMBOL_ERROR_MAX;

    for (i = 0; canvas->config.symbol_map.symbols2 [i].sym [0].c != 0; i++)
        eval_symbol_wide (canvas, wcell_a, wcell_b, i, &best_symbol, &best_eval);

    chafa_leave_mmx ();  /* Make FPU happy again */

    /* Output */

    g_assert (best_symbol >= 0);

    if (canvas->extract_colors && canvas->config.fg_only_enabled)
        eval_symbol_colors_wide (canvas, wcell_a, wcell_b,
                                 &canvas->config.symbol_map.symbols2 [best_symbol].sym [0],
                                 &canvas->config.symbol_map.symbols2 [best_symbol].sym [1],
                                 &best_eval);

    *sym_out = canvas->config.symbol_map.symbols2 [best_symbol].sym [0].c;
    *color_pair_out = best_eval.colors;

    if (error_a_out)
        *error_a_out = best_eval.error [0];
    if (error_b_out)
        *error_b_out = best_eval.error [1];
}

static void
pick_symbol_and_colors_fast (ChafaCanvas *canvas,
                             ChafaWorkCell *wcell,
                             gunichar *sym_out,
                             ChafaColorPair *color_pair_out,
                             gint *error_out)
{
    ChafaColorPair color_pair;
    guint64 bitmap;
    ChafaCandidate candidates [N_CANDIDATES_MAX];
    gint n_candidates = 0;
    SymbolEval best_eval;
    gint best_symbol;
    gint i;

    /* Generate short list of candidates */

    if (canvas->extract_colors && !canvas->config.fg_only_enabled)
    {
        chafa_work_cell_get_contrasting_color_pair (wcell, &color_pair);
    }
    else
    {
        color_pair = canvas->default_colors;
    }

    bitmap = chafa_work_cell_to_bitmap (wcell, &color_pair);
    n_candidates = CLAMP (canvas->work_factor_int, 1, N_CANDIDATES_MAX);

    chafa_symbol_map_find_candidates (&canvas->config.symbol_map,
                                      bitmap,
                                      canvas->consider_inverted,
                                      candidates, &n_candidates);

    g_assert (n_candidates > 0);

    /* Find best candidate */

    best_symbol = -1;
    best_eval.error = SYMBOL_ERROR_MAX;

    for (i = 0; i < n_candidates; i++)
        eval_symbol (canvas, wcell, candidates [i].symbol_index, &best_symbol, &best_eval);

    chafa_leave_mmx ();  /* Make FPU happy again */

    /* Output */

    g_assert (best_symbol >= 0);

    if (canvas->extract_colors && canvas->config.fg_only_enabled)
        eval_symbol_colors (canvas, wcell, &canvas->config.symbol_map.symbols [best_symbol], &best_eval);

    *sym_out = canvas->config.symbol_map.symbols [best_symbol].c;
    *color_pair_out = best_eval.colors;

    if (error_out)
        *error_out = best_eval.error;
}

static void
pick_symbol_and_colors_wide_fast (ChafaCanvas *canvas,
                                  ChafaWorkCell *wcell_a,
                                  ChafaWorkCell *wcell_b,
                                  gunichar *sym_out,
                                  ChafaColorPair *color_pair_out,
                                  gint *error_a_out,
                                  gint *error_b_out)
{
    ChafaColorPair color_pair;
    guint64 bitmaps [2];
    ChafaCandidate candidates [N_CANDIDATES_MAX];
    gint n_candidates = 0;
    SymbolEval2 best_eval;
    gint best_symbol;
    gint i;

    /* Generate short list of candidates */

    if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG
        || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG_BGFG)
    {
        color_pair = canvas->default_colors;
    }
    else
    {
        ChafaColorPair color_pair_part [2];

        chafa_work_cell_get_contrasting_color_pair (wcell_a, &color_pair_part [0]);
        chafa_work_cell_get_contrasting_color_pair (wcell_b, &color_pair_part [1]);

        color_pair.colors [0] = chafa_color_average_2 (color_pair_part [0].colors [0], color_pair_part [1].colors [0]);
        color_pair.colors [1] = chafa_color_average_2 (color_pair_part [0].colors [1], color_pair_part [1].colors [1]);
    }

    bitmaps [0] = chafa_work_cell_to_bitmap (wcell_a, &color_pair);
    bitmaps [1] = chafa_work_cell_to_bitmap (wcell_b, &color_pair);
    n_candidates = CLAMP (canvas->work_factor_int, 1, N_CANDIDATES_MAX);

    chafa_symbol_map_find_wide_candidates (&canvas->config.symbol_map,
                                           bitmaps,
                                           canvas->consider_inverted,
                                           candidates, &n_candidates);

    g_assert (n_candidates > 0);

    /* Find best candidate */

    best_symbol = -1;
    best_eval.error [0] = best_eval.error [1] = SYMBOL_ERROR_MAX;

    for (i = 0; i < n_candidates; i++)
        eval_symbol_wide (canvas, wcell_a, wcell_b, candidates [i].symbol_index,
                          &best_symbol, &best_eval);

    chafa_leave_mmx ();  /* Make FPU happy again */

    /* Output */

    g_assert (best_symbol >= 0);

    if (canvas->extract_colors && canvas->config.fg_only_enabled)
        eval_symbol_colors_wide (canvas, wcell_a, wcell_b,
                                 &canvas->config.symbol_map.symbols2 [best_symbol].sym [0],
                                 &canvas->config.symbol_map.symbols2 [best_symbol].sym [1],
                                 &best_eval);

    *sym_out = canvas->config.symbol_map.symbols2 [best_symbol].sym [0].c;
    *color_pair_out = best_eval.colors;

    if (error_a_out)
        *error_a_out = best_eval.error [0];
    if (error_b_out)
        *error_b_out = best_eval.error [1];
}

static const ChafaColor *
get_palette_color_with_color_space (ChafaPalette *palette, gint index, ChafaColorSpace cs)
{
    return chafa_palette_get_color (palette, cs, index);
}

static const ChafaColor *
get_palette_color (ChafaCanvas *canvas, ChafaPalette *palette, gint index)
{
    return get_palette_color_with_color_space (palette, index, canvas->config.color_space);
}

static void
apply_fill_fg_only (ChafaCanvas *canvas, const ChafaWorkCell *wcell, ChafaCanvasCell *cell)
{
    ChafaColor mean;
    ChafaColorCandidates ccand;
    gint fg_value, bg_value, mean_value;
    gint n_bits;
    ChafaCandidate sym_cand;
    gint n_sym_cands = 1;

    if (canvas->config.fill_symbol_map.n_symbols == 0)
        return;

    chafa_work_cell_calc_mean_color (wcell, &mean);

    if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_TRUECOLOR)
    {
        cell->fg_color = chafa_pack_color (&mean);
    }
    else
    {
        chafa_palette_lookup_nearest (&canvas->fg_palette, canvas->config.color_space, &mean, &ccand);
        cell->fg_color = ccand.index [0];
    }

    cell->bg_color = transparent_cell_color (canvas->config.canvas_mode);

    /* FIXME: Do we care enough to weight channels properly here, or convert from DIN99d?
     * Output looks acceptable without. Would have to check if it makes a noticeable
     * difference. */
    fg_value = (canvas->default_colors.colors [CHAFA_COLOR_PAIR_FG].ch [0]
                + canvas->default_colors.colors [CHAFA_COLOR_PAIR_FG].ch [1]
                + canvas->default_colors.colors [CHAFA_COLOR_PAIR_FG].ch [2])
               / 3;
    bg_value = (canvas->default_colors.colors [CHAFA_COLOR_PAIR_BG].ch [0]
                + canvas->default_colors.colors [CHAFA_COLOR_PAIR_BG].ch [1]
                + canvas->default_colors.colors [CHAFA_COLOR_PAIR_BG].ch [2])
               / 3;
    mean_value = (mean.ch [0] + mean.ch [1] + mean.ch [2]) / 3;

    n_bits = ((mean_value * 64) + 128) / 255;
    if (fg_value < bg_value)
        n_bits = 64 - n_bits;

    chafa_symbol_map_find_fill_candidates (&canvas->config.fill_symbol_map,
                                           n_bits,
                                           FALSE,
                                           &sym_cand, &n_sym_cands);

    cell->c = canvas->config.fill_symbol_map.symbols [sym_cand.symbol_index].c;
}

static void
apply_fill (ChafaCanvas *canvas, const ChafaWorkCell *wcell, ChafaCanvasCell *cell)
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

    chafa_work_cell_calc_mean_color (wcell, &mean);

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
             || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_16
             || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_8)
    {
        chafa_palette_lookup_nearest (&canvas->fg_palette, canvas->config.color_space,
                                      &mean, &ccand);
    }
    else if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_16_8)
    {
        ChafaColorCandidates ccand_bg;

        chafa_palette_lookup_nearest (&canvas->fg_palette, canvas->config.color_space,
                                      &mean, &ccand);
        chafa_palette_lookup_nearest (&canvas->bg_palette, canvas->config.color_space,
                                      &mean, &ccand_bg);

        if (ccand.index [0] != ccand_bg.index [0])
        {
            if (ccand.index [1] == ccand_bg.index [0])
                ccand.index [1] = ccand_bg.index [1];
            ccand.index [0] = ccand_bg.index [0];
        }
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

    col [0] = *get_palette_color (canvas, &canvas->fg_palette, ccand.index [0]);
    col [1] = *get_palette_color (canvas, &canvas->fg_palette, ccand.index [1]);

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

        error = chafa_color_diff_fast (&mean, &col [2]);
        if (error < best_error)
        {
            /* In FGBG mode there's no way to invert or set the BG color, so
             * assign the primary color to FG pen instead. */
            best_i = (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG ? 64 - i : i);
            best_error = error;
        }
    }

    chafa_symbol_map_find_fill_candidates (&canvas->config.fill_symbol_map, best_i,
                                           canvas->consider_inverted && canvas->config.canvas_mode != CHAFA_CANVAS_MODE_INDEXED_16_8,
                                           &sym_cand, &n_sym_cands);

    /* If we end up with a featureless symbol (space or fill), make
     * FG color equal to BG. Don't do this in FGBG mode, as it does not permit
     * color manipulation. */
    if (canvas->config.canvas_mode != CHAFA_CANVAS_MODE_FGBG
        && canvas->config.canvas_mode != CHAFA_CANVAS_MODE_INDEXED_16_8)
    {
        if (best_i == 0)
            ccand.index [1] = ccand.index [0];
        else if (best_i == 64)
            ccand.index [0] = ccand.index [1];
    }

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
quantize_colors_for_cell_16_8 (ChafaCanvas *canvas, ChafaCanvasCell *cell,
                               const ChafaColorPair *color_pair)
{
    /* First pick both colors from FG palette to see if we should eliminate the FG/BG
     * distinction. This is necessary to prevent artifacts in solid color (fg-bg-fg-bg etc). */

    /* TODO: Investigate if we could just force evaluation of the solid symbol instead. */

    cell->fg_color =
        chafa_palette_lookup_nearest (&canvas->fg_palette, canvas->config.color_space,
                                      &color_pair->colors [CHAFA_COLOR_PAIR_FG], NULL);
    cell->bg_color =
        chafa_palette_lookup_nearest (&canvas->fg_palette, canvas->config.color_space,
                                      &color_pair->colors [CHAFA_COLOR_PAIR_BG], NULL);

    if (cell->fg_color == cell->bg_color && cell->fg_color >= 8 && cell->fg_color <= 15)
    {
        /* Chosen FG and BG colors should ideally be the same, but BG palette does not allow it.
         * Use the solid char with FG color if we have one, else fall back to using the closest
         * match from the BG palette for both FG and BG. */

        if (canvas->solid_char)
        {
            cell->c = canvas->solid_char;
            cell->bg_color =
                chafa_palette_lookup_nearest (&canvas->bg_palette, canvas->config.color_space,
                                              &color_pair->colors [CHAFA_COLOR_PAIR_FG], NULL);
        }
        else
        {
            cell->fg_color = cell->bg_color =
                chafa_palette_lookup_nearest (&canvas->bg_palette, canvas->config.color_space,
                                              &color_pair->colors [CHAFA_COLOR_PAIR_FG], NULL);
        }
    }
    else
    {
        cell->bg_color = chafa_palette_lookup_nearest (&canvas->bg_palette, canvas->config.color_space,
                                                       &color_pair->colors [CHAFA_COLOR_PAIR_BG], NULL);
    }
}

static void
update_cell_colors (ChafaCanvas *canvas, ChafaCanvasCell *cell_out,
                    const ChafaColorPair *color_pair)
{
    if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_256
        || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_240
        || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_16
        || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_8
        || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG_BGFG)
    {
        cell_out->fg_color =
            chafa_palette_lookup_nearest (&canvas->fg_palette, canvas->config.color_space,
                                          &color_pair->colors [CHAFA_COLOR_PAIR_FG], NULL);
        cell_out->bg_color =
            chafa_palette_lookup_nearest (&canvas->bg_palette, canvas->config.color_space,
                                          &color_pair->colors [CHAFA_COLOR_PAIR_BG], NULL);
    }
    else if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_16_8)
    {
        quantize_colors_for_cell_16_8 (canvas, cell_out, color_pair);
    }
    else
    {
        cell_out->fg_color = chafa_pack_color (&color_pair->colors [CHAFA_COLOR_PAIR_FG]);
        cell_out->bg_color = chafa_pack_color (&color_pair->colors [CHAFA_COLOR_PAIR_BG]);
    }

    if (canvas->config.fg_only_enabled)
        cell_out->bg_color = transparent_cell_color (canvas->config.canvas_mode);
}

static gint
update_cell (ChafaCanvas *canvas, ChafaWorkCell *work_cell, ChafaCanvasCell *cell_out)
{
    gunichar sym = 0;
    ChafaColorPair color_pair;
    gint sym_error;

    if (canvas->config.symbol_map.n_symbols == 0)
        return SYMBOL_ERROR_MAX;

    if (canvas->work_factor_int >= 8)
        pick_symbol_and_colors_slow (canvas, work_cell, &sym, &color_pair, &sym_error);
    else
        pick_symbol_and_colors_fast (canvas, work_cell, &sym, &color_pair, &sym_error);

    cell_out->c = sym;
    update_cell_colors (canvas, cell_out, &color_pair);

    /* FIXME: It would probably be better to do the fgbg/bgfg blank symbol check
     * from emit_ansi_fgbg_bgfg() here. */

    return sym_error;
}

static void
update_cells_wide (ChafaCanvas *canvas, ChafaWorkCell *work_cell_a, ChafaWorkCell *work_cell_b,
                   ChafaCanvasCell *cell_a_out, ChafaCanvasCell *cell_b_out,
                   gint *error_a_out, gint *error_b_out)
{
    gunichar sym = 0;
    ChafaColorPair color_pair;

    *error_a_out = *error_b_out = SYMBOL_ERROR_MAX;

    if (canvas->config.symbol_map.n_symbols2 == 0)
        return;

    if (canvas->work_factor_int >= 8)
        pick_symbol_and_colors_wide_slow (canvas, work_cell_a, work_cell_b,
                                          &sym, &color_pair,
                                          error_a_out, error_b_out);
    else
        pick_symbol_and_colors_wide_fast (canvas, work_cell_a, work_cell_b,
                                          &sym, &color_pair,
                                          error_a_out, error_b_out);

    cell_a_out->c = sym;
    cell_b_out->c = 0;
    update_cell_colors (canvas, cell_a_out, &color_pair);
    cell_b_out->fg_color = cell_a_out->fg_color;
    cell_b_out->bg_color = cell_a_out->bg_color;

    /* quantize_colors_for_cell_16_8() can revert the char to solid, and
     * the solid char is always narrow. Extend it to both cells. */
    if (cell_a_out->c == canvas->solid_char)
        cell_b_out->c = cell_a_out->c;
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
    ChafaWorkCell work_cells [N_BUF_CELLS];
    gint cell_errors [N_BUF_CELLS];
    gint cx, cy;

    cells = &canvas->cells [row * canvas->config.width];
    cy = row;

    for (cx = 0; cx < canvas->config.width; cx++)
    {
        gint buf_index = cx % N_BUF_CELLS;
        ChafaWorkCell *wcell = &work_cells [buf_index];
        ChafaCanvasCell wide_cells [2];
        gint wide_cell_errors [2];

        memset (&cells [cx], 0, sizeof (cells [cx]));
        cells [cx].c = ' ';

        chafa_work_cell_init (wcell, canvas->pixels, canvas->width_pixels, cx, cy);
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
                cells [cx] = wide_cells [1];
                cell_errors [wide_buf_index [0]] = wide_cell_errors [0];
                cell_errors [wide_buf_index [1]] = wide_cell_errors [1];
            }
        }

        /* If we produced a featureless cell, try fill */

        /* FIXME: Check popcount == 0 or == 64 instead of symbol char */
        if (cells [cx].c != 0 && (cells [cx].c == ' ' || cells [cx].c == 0x2588
                                  || cells [cx].fg_color == cells [cx].bg_color))
        {
            if (canvas->config.fg_only_enabled)
            {
                apply_fill_fg_only (canvas, wcell, &cells [cx]);
                cells [cx].bg_color = transparent_cell_color (canvas->config.canvas_mode);
            }
            else
            {
                apply_fill (canvas, wcell, &cells [cx]);
            }
        }

        /* If cell is still featureless after fill, use blank_char consistently */

        if (cells [cx].c != 0 && (cells [cx].c == ' '
                                  || cells [cx].fg_color == cells [cx].bg_color))
        {
            cells [cx].c = canvas->blank_char;

            /* Copy FG color from previous cell in order to avoid emitting
             * unnecessary control sequences changing it, but only if we're 100%
             * sure the "blank" char has no foreground features. It's safest to
             * permit this optimization only with ASCII space. */
            if (canvas->blank_char == ' ' && cx > 0)
                cells [cx].fg_color = cells [cx - 1].fg_color;
        }
    }
}

static void
cell_build_worker (ChafaBatchInfo *batch, ChafaCanvas *canvas)
{
    gint i;

    for (i = 0; i < batch->n_rows; i++)
    {
        update_cells_row (canvas, batch->first_row + i);
    }
}

static void
update_cells (ChafaCanvas *canvas)
{
    chafa_process_batches (canvas,
                           (GFunc) cell_build_worker,
                           NULL,  /* _post */
                           canvas->config.height,
                           chafa_get_n_actual_threads (),
                           1);
}

static void
differentiate_channel (guint8 *dest_channel, guint8 reference_channel, gint min_diff)
{
    gint diff;

    diff = (gint) *dest_channel - (gint) reference_channel;

    if (diff >= -min_diff && diff <= 0)
        *dest_channel = MAX ((gint) reference_channel - min_diff, 0);
    else if (diff <= min_diff && diff >= 0)
        *dest_channel = MIN ((gint) reference_channel + min_diff, 255);
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
        chafa_color_rgb_to_din99d (&fg_col, &canvas->default_colors.colors [CHAFA_COLOR_PAIR_FG]);
        chafa_color_rgb_to_din99d (&bg_col, &canvas->default_colors.colors [CHAFA_COLOR_PAIR_BG]);
    }
    else
    {
        canvas->default_colors.colors [CHAFA_COLOR_PAIR_FG] = fg_col;
        canvas->default_colors.colors [CHAFA_COLOR_PAIR_BG] = bg_col;
    }

    canvas->default_colors.colors [CHAFA_COLOR_PAIR_FG].ch [3] = 0xff;
    canvas->default_colors.colors [CHAFA_COLOR_PAIR_BG].ch [3] = 0x00;

    /* When holding the BG, we need to compare against a consistent
     * foreground color for symbol selection by outline. 50% gray
     * yields acceptable results as a stand-in average of all possible
     * colors. The BG color can't be too similar, so push it away a
     * little if needed. This should work with both bright and dark
     * background colors, and the background color doesn't have to
     * be precise.
     *
     * We don't need to do this for monochrome modes, as they use the
     * FG/BG colors directly. */

    if (canvas->extract_colors && canvas->config.fg_only_enabled)
    {
        gint i;

        chafa_unpack_color (0xff7f7f7f,
                            &canvas->default_colors.colors [CHAFA_COLOR_PAIR_FG]);

        for (i = 0; i < 3; i++)
        {
            differentiate_channel (&canvas->default_colors.colors [CHAFA_COLOR_PAIR_BG].ch [i],
                                   canvas->default_colors.colors [CHAFA_COLOR_PAIR_FG].ch [i],
                                   5);
        }
    }
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
    ChafaPaletteType fg_pal_type = CHAFA_PALETTE_TYPE_DYNAMIC_256;
    ChafaPaletteType bg_pal_type = CHAFA_PALETTE_TYPE_DYNAMIC_256;
    ChafaColor fg_col;
    ChafaColor bg_col;

    chafa_unpack_color (canvas->config.fg_color_packed_rgb, &fg_col);
    chafa_unpack_color (canvas->config.bg_color_packed_rgb, &bg_col);

    fg_col.ch [3] = 0xff;
    bg_col.ch [3] = 0x00;

    /* The repetition here kind of sucks, but it'll get better once the
     * palette refactoring is done and subtypes go away. */

    switch (chafa_canvas_config_get_canvas_mode (&canvas->config))
    {
        case CHAFA_CANVAS_MODE_TRUECOLOR:
            fg_pal_type = CHAFA_PALETTE_TYPE_DYNAMIC_256;
            bg_pal_type = CHAFA_PALETTE_TYPE_DYNAMIC_256;
            break;

        case CHAFA_CANVAS_MODE_INDEXED_256:
            fg_pal_type = CHAFA_PALETTE_TYPE_FIXED_256;
            bg_pal_type = CHAFA_PALETTE_TYPE_FIXED_256;
            break;

        case CHAFA_CANVAS_MODE_INDEXED_240:
            fg_pal_type = CHAFA_PALETTE_TYPE_FIXED_240;
            bg_pal_type = CHAFA_PALETTE_TYPE_FIXED_240;
            break;

        case CHAFA_CANVAS_MODE_INDEXED_16:
            fg_pal_type = CHAFA_PALETTE_TYPE_FIXED_16;
            bg_pal_type = CHAFA_PALETTE_TYPE_FIXED_16;
            break;

        case CHAFA_CANVAS_MODE_INDEXED_16_8:
            fg_pal_type = CHAFA_PALETTE_TYPE_FIXED_16;
            bg_pal_type = CHAFA_PALETTE_TYPE_FIXED_8;
            break;

        case CHAFA_CANVAS_MODE_INDEXED_8:
            fg_pal_type = CHAFA_PALETTE_TYPE_FIXED_8;
            bg_pal_type = CHAFA_PALETTE_TYPE_FIXED_8;
            break;

        case CHAFA_CANVAS_MODE_FGBG_BGFG:
        case CHAFA_CANVAS_MODE_FGBG:
            fg_pal_type = CHAFA_PALETTE_TYPE_FIXED_FGBG;
            bg_pal_type = CHAFA_PALETTE_TYPE_FIXED_FGBG;
            break;

        case CHAFA_CANVAS_MODE_MAX:
            g_assert_not_reached ();
    }

    chafa_palette_init (&canvas->fg_palette, fg_pal_type);

    chafa_palette_set_color (&canvas->fg_palette, CHAFA_PALETTE_INDEX_FG, &fg_col);
    chafa_palette_set_color (&canvas->fg_palette, CHAFA_PALETTE_INDEX_BG, &bg_col);

    chafa_palette_set_alpha_threshold (&canvas->fg_palette, canvas->config.alpha_threshold);
    chafa_palette_set_transparent_index (&canvas->fg_palette, CHAFA_PALETTE_INDEX_TRANSPARENT);

    chafa_palette_init (&canvas->bg_palette, bg_pal_type);

    chafa_palette_set_color (&canvas->bg_palette, CHAFA_PALETTE_INDEX_FG, &fg_col);
    chafa_palette_set_color (&canvas->bg_palette, CHAFA_PALETTE_INDEX_BG, &bg_col);

    chafa_palette_set_alpha_threshold (&canvas->bg_palette, canvas->config.alpha_threshold);
    chafa_palette_set_transparent_index (&canvas->bg_palette, CHAFA_PALETTE_INDEX_TRANSPARENT);
}

static gunichar
find_best_blank_char (ChafaCanvas *canvas)
{
    ChafaCandidate candidates [N_CANDIDATES_MAX];
    gint n_candidates;
    gunichar best_char = 0x20;

    /* Try space (0x20) first */
    if (chafa_symbol_map_has_symbol (&canvas->config.symbol_map, 0x20)
        || chafa_symbol_map_has_symbol (&canvas->config.fill_symbol_map, 0x20))
        return 0x20;

    n_candidates = N_CANDIDATES_MAX;
    chafa_symbol_map_find_fill_candidates (&canvas->config.fill_symbol_map,
                                           0,
                                           FALSE,
                                           candidates,
                                           &n_candidates);
    if (n_candidates > 0)
    {
        best_char = canvas->config.fill_symbol_map.symbols [candidates [0].symbol_index].c;
    }
    else
    {
        n_candidates = N_CANDIDATES_MAX;
        chafa_symbol_map_find_candidates (&canvas->config.symbol_map,
                                          0,
                                          FALSE,
                                          candidates,
                                          &n_candidates);
        if (n_candidates > 0)
        {
            best_char = canvas->config.symbol_map.symbols [candidates [0].symbol_index].c;
        }
    }

    return best_char;
}

static gunichar
find_best_solid_char (ChafaCanvas *canvas)
{
    ChafaCandidate candidates [N_CANDIDATES_MAX];
    gint n_candidates;
    gunichar best_char = 0;

    /* Try solid block (0x2588) first */
    if (chafa_symbol_map_has_symbol (&canvas->config.symbol_map, 0x2588)
        || chafa_symbol_map_has_symbol (&canvas->config.fill_symbol_map, 0x2588))
        return 0x2588;

    n_candidates = N_CANDIDATES_MAX;
    chafa_symbol_map_find_fill_candidates (&canvas->config.fill_symbol_map,
                                           64,
                                           FALSE,
                                           candidates,
                                           &n_candidates);
    if (n_candidates > 0 && candidates [0].hamming_distance <= 32)
    {
        best_char = canvas->config.fill_symbol_map.symbols [candidates [0].symbol_index].c;
    }
    else
    {
        n_candidates = N_CANDIDATES_MAX;
        chafa_symbol_map_find_candidates (&canvas->config.symbol_map,
                                          0xffffffffffffffff,
                                          FALSE,
                                          candidates,
                                          &n_candidates);
        if (n_candidates > 0 && candidates [0].hamming_distance <= 32)
        {
            best_char = canvas->config.symbol_map.symbols [candidates [0].symbol_index].c;
        }
    }

    return best_char;
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
    else if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_SIXELS)
    {
        /* Sixels */
        canvas->width_pixels = canvas->config.width * canvas->config.cell_width;
        canvas->height_pixels = canvas->config.height * canvas->config.cell_height;

        /* Ensure height is the biggest multiple of 6 that will fit
         * in our cells. We don't want a fringe going outside our
         * bottom cell. */
        canvas->height_pixels -= canvas->height_pixels % 6;
    }
    else  /* CHAFA_PIXEL_MODE_KITTY or CHAFA_PIXEL_MODE_ITERM2 */
    {
        canvas->width_pixels = canvas->config.width * canvas->config.cell_width;
        canvas->height_pixels = canvas->config.height * canvas->config.cell_height;
    }

    canvas->pixels = NULL;
    canvas->cells = g_new (ChafaCanvasCell, canvas->config.width * canvas->config.height);
    canvas->work_factor_int = canvas->config.work_factor * 10 + 0.5f;
    canvas->needs_clear = TRUE;
    canvas->have_alpha = FALSE;

    canvas->consider_inverted = !(canvas->config.fg_only_enabled
                                  || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG);

    canvas->extract_colors = !(canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG
                               || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG_BGFG);

    if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG)
        canvas->config.fg_only_enabled = TRUE;

    canvas->use_quantized_error =
        (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_16_8
         && !canvas->config.fg_only_enabled);

    chafa_symbol_map_prepare (&canvas->config.symbol_map);
    chafa_symbol_map_prepare (&canvas->config.fill_symbol_map);

    canvas->blank_char = find_best_blank_char (canvas);
    canvas->solid_char = find_best_solid_char (canvas);

    /* In truecolor mode we don't support any fancy color spaces for now, since
     * we'd have to convert back to RGB space when emitting control codes, and
     * the code for that has yet to be written. In palette modes we just use
     * the palette mappings.
     *
     * There is also no reason to dither in truecolor mode, _unless_ we're
     * producing sixels, which quantize to a dynamic palette. */
    if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_KITTY
        || canvas->config.pixel_mode == CHAFA_PIXEL_MODE_ITERM2
        || (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_TRUECOLOR
            && canvas->config.pixel_mode == CHAFA_PIXEL_MODE_SYMBOLS))
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
            case CHAFA_CANVAS_MODE_INDEXED_16_8:
                dither_intensity = DITHER_BASE_INTENSITY_16C;
                break;
            case CHAFA_CANVAS_MODE_INDEXED_8:
                dither_intensity = DITHER_BASE_INTENSITY_8C;
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

static void
destroy_pixel_canvas (ChafaCanvas *canvas)
{
    if (canvas->pixel_canvas)
    {
        if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_SIXELS)
            chafa_sixel_canvas_destroy (canvas->pixel_canvas);
        else if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_KITTY)
            chafa_kitty_canvas_destroy (canvas->pixel_canvas);
        else if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_ITERM2)
            chafa_iterm2_canvas_destroy (canvas->pixel_canvas);

        canvas->pixel_canvas = NULL;
    }
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
        destroy_pixel_canvas (canvas);
        chafa_dither_deinit (&canvas->dither);
        chafa_palette_deinit (&canvas->fg_palette);
        chafa_palette_deinit (&canvas->bg_palette);
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

    destroy_pixel_canvas (canvas);

    if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_SYMBOLS)
    {
        /* Symbol mode */

        canvas->pixels = g_new (ChafaPixel, canvas->width_pixels * canvas->height_pixels);

        chafa_prepare_pixel_data_for_symbols (&canvas->fg_palette, &canvas->dither,
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
    else if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_SIXELS)
    {
        /* Sixel mode */

        canvas->fg_palette.alpha_threshold = canvas->config.alpha_threshold;
        canvas->pixel_canvas = chafa_sixel_canvas_new (canvas->width_pixels,
                                                       canvas->height_pixels,
                                                       canvas->config.color_space,
                                                       &canvas->fg_palette,
                                                       &canvas->dither);
        chafa_sixel_canvas_draw_all_pixels (canvas->pixel_canvas,
                                            src_pixel_type,
                                            src_pixels,
                                            src_width, src_height,
                                            src_rowstride);
    }
    else if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_KITTY)
    {
        /* Kitty mode */

        canvas->fg_palette.alpha_threshold = canvas->config.alpha_threshold;
        canvas->pixel_canvas = chafa_kitty_canvas_new (canvas->width_pixels,
                                                       canvas->height_pixels);
        chafa_kitty_canvas_draw_all_pixels (canvas->pixel_canvas,
                                            src_pixel_type,
                                            src_pixels,
                                            src_width, src_height,
                                            src_rowstride);
    }
    else  /* if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_ITERM2) */
    {
        /* iTerm2 mode */

        canvas->fg_palette.alpha_threshold = canvas->config.alpha_threshold;
        canvas->pixel_canvas = chafa_iterm2_canvas_new (canvas->width_pixels,
                                                        canvas->height_pixels);
        chafa_iterm2_canvas_draw_all_pixels (canvas->pixel_canvas,
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
 *
 * Deprecated: 1.6: Use chafa_canvas_print() instead.
 **/
GString *
chafa_canvas_build_ansi (ChafaCanvas *canvas)
{
    g_return_val_if_fail (canvas != NULL, NULL);
    g_return_val_if_fail (canvas->refs > 0, NULL);

    return chafa_canvas_print (canvas, NULL);
}

/**
 * chafa_canvas_print:
 * @canvas: The canvas to generate a printable representation of
 * @term_info: Terminal to format for, or %NULL for fallback
 *
 * Builds a UTF-8 string of terminal control sequences and symbols
 * representing the canvas' current contents. This can e.g. be printed
 * to a terminal. The exact choice of escape sequences and symbols,
 * dimensions, etc. is determined by the configuration assigned to
 * @canvas on its creation.
 *
 * All output lines except for the last one will end in a newline.
 *
 * Returns: A UTF-8 string of terminal control sequences and symbols
 *
 * Since: 1.6
 **/
GString *
chafa_canvas_print (ChafaCanvas *canvas, ChafaTermInfo *term_info)
{
    GString *str;

    g_return_val_if_fail (canvas != NULL, NULL);
    g_return_val_if_fail (canvas->refs > 0, NULL);

    if (term_info)
        chafa_term_info_ref (term_info);
    else
        term_info = chafa_term_db_get_fallback_info (chafa_term_db_get_default ());

    if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_SYMBOLS)
    {
        maybe_clear (canvas);
        str = chafa_canvas_print_symbols (canvas, term_info);
    }
    else if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_SIXELS
             && chafa_term_info_get_seq (term_info, CHAFA_TERM_SEQ_BEGIN_SIXELS))
    {
        gchar buf [CHAFA_TERM_SEQ_LENGTH_MAX + 1];
        gchar *out;

        /* Sixel mode */

        out = chafa_term_info_emit_begin_sixels (term_info, buf, 0, 1, 0);
        *out = '\0';
        str = g_string_new (buf);

        g_string_append_printf (str, "\"1;1;%d;%d", canvas->width_pixels, canvas->height_pixels);
        chafa_sixel_canvas_build_ansi (canvas->pixel_canvas, str);

        out = chafa_term_info_emit_end_sixels (term_info, buf);
        *out = '\0';
        g_string_append (str, buf);
    }
    else if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_KITTY
             && chafa_term_info_get_seq (term_info, CHAFA_TERM_SEQ_BEGIN_KITTY_IMMEDIATE_IMAGE_V1))
    {
        /* Kitty mode */

        str = g_string_new ("");
        chafa_kitty_canvas_build_ansi (canvas->pixel_canvas, term_info, str,
                                       canvas->config.width, canvas->config.height);
    }
    else if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_ITERM2)
    {
        /* iTerm2 mode */

        str = g_string_new ("");
        chafa_iterm2_canvas_build_ansi (canvas->pixel_canvas, term_info, str,
                                        canvas->config.width, canvas->config.height);
    }
    else
    {
        str = g_string_new ("");
    }

    chafa_term_info_unref (term_info);
    return str;
}

/**
 * chafa_canvas_get_char_at:
 * @canvas: The canvas to inspect
 * @x: Column of character cell to inspect
 * @y: Row of character cell to inspect
 *
 * Returns the character at cell (x, y). The coordinates are zero-indexed. For
 * double-width characters, the leftmost cell will contain the character
 * and the rightmost cell will contain 0.
 *
 * Returns: The character at (x, y)
 *
 * Since: 1.8
 **/
gunichar
chafa_canvas_get_char_at (ChafaCanvas *canvas, gint x, gint y)
{
    g_return_val_if_fail (canvas != NULL, 0);
    g_return_val_if_fail (canvas->refs > 0, 0);
    g_return_val_if_fail (x >= 0 && x < canvas->config.width, 0);
    g_return_val_if_fail (y >= 0 && y < canvas->config.height, 0);

    return canvas->cells [y * canvas->config.width + x].c;
}

/**
 * chafa_canvas_set_char_at:
 * @canvas: The canvas to manipulate
 * @x: Column of character cell to manipulate
 * @y: Row of character cell to manipulate
 * @c: The character value to store
 *
 * Sets the character at cell (x, y). The coordinates are zero-indexed. For
 * double-width characters, the leftmost cell must contain the character
 * and the cell to the right of it will automatically be set to 0.
 *
 * If the character is a nonprintable or zero-width, no change will be
 * made.
 *
 * Returns: The number of cells output (0, 1 or 2)
 *
 * Since: 1.8
 **/
gint
chafa_canvas_set_char_at (ChafaCanvas *canvas, gint x, gint y, gunichar c)
{
    ChafaCanvasCell *cell;
    gint cwidth = 1;

    g_return_val_if_fail (canvas != NULL, 0);
    g_return_val_if_fail (canvas->refs > 0, 0);
    g_return_val_if_fail (x >= 0 && x < canvas->config.width, 0);
    g_return_val_if_fail (y >= 0 && y < canvas->config.height, 0);

    if (!g_unichar_isprint (c) || g_unichar_iszerowidth (c))
        return 0;

    if (g_unichar_iswide (c))
        cwidth = 2;

    if (x + cwidth > canvas->config.width)
        return 0;

    cell = &canvas->cells [y * canvas->config.width + x];
    cell [0].c = c;

    if (cwidth == 2)
    {
        cell [1].c = 0;
        cell [1].fg_color = cell [0].fg_color;
        cell [1].bg_color = cell [0].bg_color;
    }

    /* If we're overwriting the rightmost half of a wide character,
     * clear its leftmost half */

    if (x > 0)
    {
        if (cell [-1].c != 0
            && g_unichar_iswide (cell [-1].c))
            cell [-1].c = canvas->blank_char;
    }

    return cwidth;
}

/**
 * chafa_canvas_get_colors_at:
 * @canvas: The canvas to inspect
 * @x: Column of character cell to inspect
 * @y: Row of character cell to inspect
 * @fg_out: Storage for foreground color
 * @bg_out: Storage for background color
 *
 * Gets the colors at cell (x, y). The coordinates are zero-indexed. For
 * double-width characters, both cells will contain the same colors.
 *
 * The colors will be -1 for transparency, packed 8bpc RGB otherwise,
 * i.e. 0x00RRGGBB hex.
 *
 * If the canvas is in an indexed mode, palette lookups will be made
 * for you.
 *
 * Since: 1.8
 **/
void
chafa_canvas_get_colors_at (ChafaCanvas *canvas, gint x, gint y,
                            gint *fg_out, gint *bg_out)
{
    const ChafaCanvasCell *cell;
    gint fg = -1, bg = -1;

    g_return_if_fail (canvas != NULL);
    g_return_if_fail (canvas->refs > 0);
    g_return_if_fail (x >= 0 && x < canvas->config.width);
    g_return_if_fail (y >= 0 && y < canvas->config.height);

    cell = &canvas->cells [y * canvas->config.width + x];

    switch (canvas->config.canvas_mode)
    {
        case CHAFA_CANVAS_MODE_TRUECOLOR:
            fg = packed_rgba_to_rgb (canvas, cell->fg_color);
            bg = packed_rgba_to_rgb (canvas, cell->bg_color);
            break;
        case CHAFA_CANVAS_MODE_INDEXED_256:
        case CHAFA_CANVAS_MODE_INDEXED_240:
        case CHAFA_CANVAS_MODE_INDEXED_16:
        case CHAFA_CANVAS_MODE_INDEXED_16_8:
        case CHAFA_CANVAS_MODE_INDEXED_8:
        case CHAFA_CANVAS_MODE_FGBG_BGFG:
        case CHAFA_CANVAS_MODE_FGBG:
            if (cell->fg_color == CHAFA_PALETTE_INDEX_BG
                || cell->fg_color == CHAFA_PALETTE_INDEX_TRANSPARENT)
                fg = -1;
            else
                fg = color_to_rgb (canvas,
                                   *get_palette_color_with_color_space (&canvas->fg_palette, cell->fg_color,
                                                                        CHAFA_COLOR_SPACE_RGB));
            if (cell->bg_color == CHAFA_PALETTE_INDEX_BG
                || cell->bg_color == CHAFA_PALETTE_INDEX_TRANSPARENT)
                bg = -1;
            else
                bg = color_to_rgb (canvas,
                                   *get_palette_color_with_color_space (&canvas->bg_palette, cell->bg_color,
                                                                        CHAFA_COLOR_SPACE_RGB));
            break;
        case CHAFA_CANVAS_MODE_MAX:
            g_assert_not_reached ();
            break;
    }

    *fg_out = fg;
    *bg_out = bg;
}

/**
 * chafa_canvas_set_colors_at:
 * @canvas: The canvas to manipulate
 * @x: Column of character cell to manipulate
 * @y: Row of character cell to manipulate
 * @fg: Foreground color
 * @bg: Background color
 *
 * Sets the colors at cell (x, y). The coordinates are zero-indexed. For
 * double-width characters, both cells will be set to the same color.
 *
 * The colors must be -1 for transparency, packed 8bpc RGB otherwise,
 * i.e. 0x00RRGGBB hex.
 *
 * If the canvas is in an indexed mode, palette lookups will be made
 * for you.
 *
 * Since: 1.8
 **/
void
chafa_canvas_set_colors_at (ChafaCanvas *canvas, gint x, gint y,
                            gint fg, gint bg)
{
    ChafaCanvasCell *cell;

    g_return_if_fail (canvas != NULL);
    g_return_if_fail (canvas->refs > 0);
    g_return_if_fail (x >= 0 && x < canvas->config.width);
    g_return_if_fail (y >= 0 && y < canvas->config.height);

    cell = &canvas->cells [y * canvas->config.width + x];

    switch (canvas->config.canvas_mode)
    {
        case CHAFA_CANVAS_MODE_TRUECOLOR:
            cell->fg_color = packed_rgb_to_rgba (fg);
            cell->bg_color = packed_rgb_to_rgba (bg);
            break;
        case CHAFA_CANVAS_MODE_INDEXED_256:
        case CHAFA_CANVAS_MODE_INDEXED_240:
        case CHAFA_CANVAS_MODE_INDEXED_16:
        case CHAFA_CANVAS_MODE_INDEXED_16_8:
        case CHAFA_CANVAS_MODE_INDEXED_8:
            cell->fg_color = packed_rgb_to_index (&canvas->fg_palette, canvas->config.color_space, fg);
            cell->bg_color = packed_rgb_to_index (&canvas->bg_palette, canvas->config.color_space, bg);
            break;
        case CHAFA_CANVAS_MODE_FGBG_BGFG:
            cell->fg_color = fg >= 0 ? CHAFA_PALETTE_INDEX_FG : CHAFA_PALETTE_INDEX_TRANSPARENT;
            cell->bg_color = bg >= 0 ? CHAFA_PALETTE_INDEX_FG : CHAFA_PALETTE_INDEX_TRANSPARENT;
            break;
        case CHAFA_CANVAS_MODE_FGBG:
            cell->fg_color = fg >= 0 ? fg : CHAFA_PALETTE_INDEX_TRANSPARENT;
            break;
        case CHAFA_CANVAS_MODE_MAX:
            g_assert_not_reached ();
            break;
    }

    /* If setting the color of half a wide char, set it for the other half too */

    if (x > 0 && cell->c == 0)
    {
        cell [-1].fg_color = cell->fg_color;
        cell [-1].bg_color = cell->bg_color;
    }

    if (x < canvas->config.width - 1 && cell [1].c == 0)
    {
        cell [1].fg_color = cell->fg_color;
        cell [1].bg_color = cell->bg_color;
    }
}

/**
 * chafa_canvas_get_raw_colors_at:
 * @canvas: The canvas to inspect
 * @x: Column of character cell to inspect
 * @y: Row of character cell to inspect
 * @fg_out: Storage for foreground color
 * @bg_out: Storage for background color
 *
 * Gets the colors at cell (x, y). The coordinates are zero-indexed. For
 * double-width characters, both cells will contain the same colors.
 *
 * The colors will be -1 for transparency, packed 8bpc RGB, i.e.
 * 0x00RRGGBB hex in truecolor mode, or the raw pen value (0-255) in
 * indexed modes.
 *
 * It's the caller's responsibility to handle the color values correctly
 * according to the canvas mode (truecolor or indexed).
 *
 * Since: 1.8
 **/
void
chafa_canvas_get_raw_colors_at (ChafaCanvas *canvas, gint x, gint y,
                                gint *fg_out, gint *bg_out)
{
    const ChafaCanvasCell *cell;
    gint fg = -1, bg = -1;

    g_return_if_fail (canvas != NULL);
    g_return_if_fail (canvas->refs > 0);
    g_return_if_fail (x >= 0 && x < canvas->config.width);
    g_return_if_fail (y >= 0 && y < canvas->config.height);

    cell = &canvas->cells [y * canvas->config.width + x];

    switch (canvas->config.canvas_mode)
    {
        case CHAFA_CANVAS_MODE_TRUECOLOR:
            fg = packed_rgba_to_rgb (canvas, cell->fg_color);
            bg = packed_rgba_to_rgb (canvas, cell->bg_color);
            break;
        case CHAFA_CANVAS_MODE_INDEXED_256:
        case CHAFA_CANVAS_MODE_INDEXED_240:
        case CHAFA_CANVAS_MODE_INDEXED_16:
        case CHAFA_CANVAS_MODE_INDEXED_16_8:
        case CHAFA_CANVAS_MODE_INDEXED_8:
            fg = cell->fg_color < 256 ? (gint) cell->fg_color : -1;
            bg = cell->bg_color < 256 ? (gint) cell->bg_color : -1;
            break;
        case CHAFA_CANVAS_MODE_FGBG_BGFG:
            fg = cell->fg_color == CHAFA_PALETTE_INDEX_FG ? 0 : -1;
            bg = cell->bg_color == CHAFA_PALETTE_INDEX_FG ? 0 : -1;
            break;
        case CHAFA_CANVAS_MODE_FGBG:
            fg = 0;
            bg = -1;
            break;
        case CHAFA_CANVAS_MODE_MAX:
            g_assert_not_reached ();
            break;
    }

    if (fg_out)
        *fg_out = fg;
    if (bg_out)
        *bg_out = bg;
}

/**
 * chafa_canvas_set_raw_colors_at:
 * @canvas: The canvas to manipulate
 * @x: Column of character cell to manipulate
 * @y: Row of character cell to manipulate
 * @fg: Foreground color
 * @bg: Background color
 *
 * Sets the colors at cell (x, y). The coordinates are zero-indexed. For
 * double-width characters, both cells will be set to the same color.
 *
 * The colors must be -1 for transparency, packed 8bpc RGB, i.e.
 * 0x00RRGGBB hex in truecolor mode, or the raw pen value (0-255) in
 * indexed modes.
 *
 * It's the caller's responsibility to handle the color values correctly
 * according to the canvas mode (truecolor or indexed).
 *
 * Since: 1.8
 **/
void
chafa_canvas_set_raw_colors_at (ChafaCanvas *canvas, gint x, gint y,
                                gint fg, gint bg)
{
    ChafaCanvasCell *cell;

    g_return_if_fail (canvas != NULL);
    g_return_if_fail (canvas->refs > 0);
    g_return_if_fail (x >= 0 && x < canvas->config.width);
    g_return_if_fail (y >= 0 && y < canvas->config.height);

    cell = &canvas->cells [y * canvas->config.width + x];

    switch (canvas->config.canvas_mode)
    {
        case CHAFA_CANVAS_MODE_TRUECOLOR:
            cell->fg_color = packed_rgb_to_rgba (fg);
            cell->bg_color = packed_rgb_to_rgba (bg);
            break;
        case CHAFA_CANVAS_MODE_INDEXED_256:
        case CHAFA_CANVAS_MODE_INDEXED_240:
        case CHAFA_CANVAS_MODE_INDEXED_16:
        case CHAFA_CANVAS_MODE_INDEXED_16_8:
        case CHAFA_CANVAS_MODE_INDEXED_8:
            cell->fg_color = fg >= 0 ? fg : CHAFA_PALETTE_INDEX_TRANSPARENT;
            cell->bg_color = bg >= 0 ? bg : CHAFA_PALETTE_INDEX_TRANSPARENT;
            break;
        case CHAFA_CANVAS_MODE_FGBG_BGFG:
            cell->fg_color = fg >= 0 ? CHAFA_PALETTE_INDEX_FG : CHAFA_PALETTE_INDEX_TRANSPARENT;
            cell->bg_color = bg >= 0 ? CHAFA_PALETTE_INDEX_FG : CHAFA_PALETTE_INDEX_TRANSPARENT;
            break;
        case CHAFA_CANVAS_MODE_FGBG:
            cell->fg_color = fg >= 0 ? fg : CHAFA_PALETTE_INDEX_TRANSPARENT;
            break;
        case CHAFA_CANVAS_MODE_MAX:
            g_assert_not_reached ();
            break;
    }

    /* If setting the color of half a wide char, set it for the other half too */

    if (x > 0 && cell->c == 0)
    {
        cell [-1].fg_color = cell->fg_color;
        cell [-1].bg_color = cell->bg_color;
    }

    if (x < canvas->config.width - 1 && cell [1].c == 0)
    {
        cell [1].fg_color = cell->fg_color;
        cell [1].bg_color = cell->bg_color;
    }
}
