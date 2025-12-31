/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2025-2026 Hans Petter Jansson
 *
 * This file is part of Chafa, a program that shows pictures on text terminals.
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
#include "internal/chafa-symbol-renderer.h"
#include "internal/chafa-work-cell.h"
#include "internal/smolscale/smolscale.h"

/* Used for cell initialization. May be added up over multiple cells, so a
 * low multiple needs to fit in an integer. */
#define SYMBOL_ERROR_MAX (G_MAXINT / 8)

/* Max candidates to consider in pick_symbol_and_colors_fast(). This is also
 * limited by a similar constant in chafa-symbol-map.c */
#define N_CANDIDATES_MAX 8

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
calc_cell_error_plain (const ChafaPixel *block, const ChafaColorPair *color_pair, const guint8 *cov)
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

#ifdef HAVE_AVX2_INTRINSICS
    if (chafa_have_avx2 ())
        error = chafa_calc_cell_error_avx2 (wcell->pixels, &pair, sym->mask_u32);
    else
#endif
#ifdef HAVE_SSE41_INTRINSICS
    if (chafa_have_sse41 ())
        error = chafa_calc_cell_error_sse41 (wcell->pixels, &pair, covp);
    else
#endif
        error = calc_cell_error_plain (wcell->pixels, &pair, covp);

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
            {
                cells [cx].fg_color = cells [cx - 1].fg_color;

                /* We may use inverted colors when the foreground is transparent.
                 * Some downstream tools don't handle this and will keep
                 * modulating the wrong pen. In order to suppress long runs of
                 * artifacts, make the (unused) foreground pen opaque (gh#108). */
                if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_TRUECOLOR)
                    cells [cx].fg_color |= 0xff000000;
                else if (cells [cx].fg_color == CHAFA_PALETTE_INDEX_TRANSPARENT)
                    cells [cx].fg_color = CHAFA_PALETTE_INDEX_FG;
            }
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

ChafaSymbolRenderer *
chafa_symbol_renderer_new (ChafaCanvas *canvas,
			   gint x, gint y,
			   gint width, gint height)
{
    ChafaSymbolRenderer *renderer;

    renderer = g_new0 (ChafaSymbolRenderer, 1);
    renderer->canvas = canvas;
    return renderer;
}

void
chafa_symbol_renderer_destroy (ChafaSymbolRenderer *renderer)
{
    g_free (renderer);
}

void
chafa_symbol_renderer_draw_all_pixels (ChafaSymbolRenderer *renderer,
				       ChafaPixelType src_pixel_type,
				       gconstpointer src_pixels,
				       gint src_width, gint src_height, gint src_rowstride,
				       ChafaAlign halign, ChafaAlign valign,
				       ChafaTuck tuck,
				       gfloat quality)
{
    ChafaCanvas *canvas;

    canvas = renderer->canvas;

    /* FIXME: The allocation can fail if the canvas is ridiculously large.
     * Since there's no way to report an error from here, we'll silently
     * skip the update instead.
     *
     * We really shouldn't need this much temporary memory in the first place;
     * it'd be possible to process the image in cell_height strips and hand
     * each strip off to the update_cells() pass independently. The pipelining
     * would improve throughput too. */

    canvas->pixels = g_try_new (ChafaPixel, (gsize) canvas->width_pixels * canvas->height_pixels);
    if (canvas->pixels)
    {
	chafa_prepare_pixel_data_for_symbols (&canvas->fg_palette, &canvas->dither,
					      canvas->config.color_space,
					      canvas->config.preprocessing_enabled,
					      canvas->work_factor_int,
					      src_pixel_type,
					      src_pixels,
					      src_width, src_height,
					      src_rowstride,
					      canvas->pixels,
					      canvas->width_pixels, canvas->height_pixels,
					      canvas->config.cell_width,
					      canvas->config.cell_height,
					      halign, valign,
					      tuck);

	if (canvas->config.alpha_threshold == 0)
	    canvas->have_alpha = FALSE;

	update_cells (canvas);
	canvas->needs_clear = FALSE;

	g_free (canvas->pixels);
	canvas->pixels = NULL;
    }
    else
    {
#if 0
	g_warning ("ChafaCanvas: Out of memory allocating %ux%u pixels.",
		   canvas->width_pixels, canvas->height_pixels);
#endif
    }
}
