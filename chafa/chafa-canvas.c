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
calc_mean_color (ChafaPixel *block, ChafaColor *color_out)
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
    gint i;

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
    gint i;

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
pick_symbol_and_colors_slow (ChafaCanvas *canvas, gint cx, gint cy,
                             gunichar *sym_out,
                             ChafaColor *fg_col_out,
                             ChafaColor *bg_col_out,
                             gint *error_out)
{
    ChafaPixel block [CHAFA_SYMBOL_N_PIXELS];
    SymbolEval eval [CHAFA_N_SYMBOLS_MAX] = { 0 };
    gint n;
    gint i;

    fetch_canvas_pixel_block (canvas, cx, cy, block);

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
            ChafaColor fg_col, bg_col;

            eval_symbol_colors (block, &canvas->config.symbol_map.symbols [i], &eval [i]);

            /* Threshold alpha */

            if (eval [i].fg.col.ch [3] < canvas->config.alpha_threshold)
                eval [i].fg.col.ch [3] = 0x00;
            else
                eval [i].fg.col.ch [3] = 0xff;

            if (eval [i].bg.col.ch [3] < canvas->config.alpha_threshold)
                eval [i].bg.col.ch [3] = 0x00;
            else
                eval [i].bg.col.ch [3] = 0xff;

            fg_col = eval [i].fg.col;
            bg_col = eval [i].bg.col;

            /* Pick palette colors before error evaluation; this improves
             * fine detail fidelity slightly. */

            if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_16)
            {
                fg_col = *chafa_get_palette_color_256 (chafa_pick_color_16 (&eval [i].fg.col,
                                                                            canvas->config.color_space), canvas->config.color_space);
                bg_col = *chafa_get_palette_color_256 (chafa_pick_color_16 (&eval [i].bg.col,
                                                                            canvas->config.color_space), canvas->config.color_space);
            }
            else if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_240)
            {
                fg_col = *chafa_get_palette_color_256 (chafa_pick_color_240 (&eval [i].fg.col,
                                                                             canvas->config.color_space), canvas->config.color_space);
                bg_col = *chafa_get_palette_color_256 (chafa_pick_color_240 (&eval [i].bg.col,
                                                                             canvas->config.color_space), canvas->config.color_space);
            }
            else if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_256)
            {
                fg_col = *chafa_get_palette_color_256 (chafa_pick_color_256 (&eval [i].fg.col,
                                                                             canvas->config.color_space), canvas->config.color_space);
                bg_col = *chafa_get_palette_color_256 (chafa_pick_color_256 (&eval [i].bg.col,
                                                                             canvas->config.color_space), canvas->config.color_space);
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
pick_symbol_and_colors_fast (ChafaCanvas *canvas, gint cx, gint cy,
                             gunichar *sym_out,
                             ChafaColor *fg_col_out,
                             ChafaColor *bg_col_out,
                             gint *error_out)
{
    ChafaPixel block [CHAFA_SYMBOL_N_PIXELS];
    ChafaColor color_pair [2];
    guint64 bitmap;
    ChafaCandidate candidates [N_CANDIDATES_MAX];
    SymbolEval eval [N_CANDIDATES_MAX];
    gint n_candidates = 0;
    gint best_candidate = 0;
    gint best_error = G_MAXINT;
    gint i;

    fetch_canvas_pixel_block (canvas, cx, cy, block);

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

static void
update_cells_row (ChafaCanvas *canvas, gint row)
{
    gint cx, cy;
    gint i = 0;

    cy = row;
    i = row * canvas->config.width;

    for (cx = 0; cx < canvas->config.width; cx++, i++)
    {
        ChafaCanvasCell *cell = &canvas->cells [i];
        gunichar sym;
        ChafaColor fg_col, bg_col;

        if (canvas->work_factor_int >= 8)
            pick_symbol_and_colors_slow (canvas, cx, cy, &sym, &fg_col, &bg_col, NULL);
        else
            pick_symbol_and_colors_fast (canvas, cx, cy, &sym, &fg_col, &bg_col, NULL);

        cell->c = sym;

        if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_256)
        {
            cell->fg_color = chafa_pick_color_256 (&fg_col, canvas->config.color_space);
            cell->bg_color = chafa_pick_color_256 (&bg_col, canvas->config.color_space);
        }
        else if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_240)
        {
            cell->fg_color = chafa_pick_color_240 (&fg_col, canvas->config.color_space);
            cell->bg_color = chafa_pick_color_240 (&bg_col, canvas->config.color_space);
        }
        else if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_16)
        {
            cell->fg_color = chafa_pick_color_16 (&fg_col, canvas->config.color_space);
            cell->bg_color = chafa_pick_color_16 (&bg_col, canvas->config.color_space);
        }
        else if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG_BGFG)
        {
            cell->fg_color = chafa_pick_color_fgbg (&fg_col, canvas->config.color_space, &canvas->fg_color, &canvas->bg_color);
            cell->bg_color = chafa_pick_color_fgbg (&bg_col, canvas->config.color_space, &canvas->fg_color, &canvas->bg_color);
        }
        else
        {
            cell->fg_color = chafa_pack_color (&fg_col);
            cell->bg_color = chafa_pack_color (&bg_col);
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

/* Wrong but cheap function to recenter pixel values around the mean using
 * saturation addition. Greatly improves image definition in two-color
 * modes (FGBG and FGBG_BGFG). */
static void
adjust_levels_rgb (ChafaCanvas *canvas)
{
    ChafaPixel *p0, *p1;
    gint64 accum = 0;
    gint chadd;

    p0 = canvas->pixels;
    p1 = p0 + canvas->width_pixels * canvas->height_pixels;

    for (p0 = canvas->pixels; p0 < p1; p0++)
    {
        accum += p0->col.ch [0];
        accum += p0->col.ch [1];
        accum += p0->col.ch [2];
    }

    accum /= (canvas->width_pixels * canvas->height_pixels * 3);
    chadd = 128 - accum;
    chadd = CLAMP (chadd, -100, 100);

    for (p0 = canvas->pixels; p0 < p1; p0++)
    {
        p0->col.ch [0] += chadd;
        p0->col.ch [0] = CLAMP (p0->col.ch [0], 0, 255);
        p0->col.ch [1] += chadd;
        p0->col.ch [1] = CLAMP (p0->col.ch [1], 0, 255);
        p0->col.ch [2] += chadd;
        p0->col.ch [2] = CLAMP (p0->col.ch [2], 0, 255);
    }
}

/* See description of adjust_levels_rgb () */
static void
adjust_levels_din99d (ChafaCanvas *canvas)
{
    ChafaPixel *p0, *p1;
    gint64 accum = 0;
    gint chadd;

    p0 = canvas->pixels;
    p1 = p0 + canvas->width_pixels * canvas->height_pixels;

    for (p0 = canvas->pixels; p0 < p1; p0++)
    {
        accum += p0->col.ch [0];
        accum += p0->col.ch [1];
    }

    accum /= (canvas->width_pixels * canvas->height_pixels * 2);
    chadd = 128 - accum;
    chadd = CLAMP (chadd, -100, 100);

    for (p0 = canvas->pixels; p0 < p1; p0++)
    {
        p0->col.ch [0] += chadd;
        p0->col.ch [0] = CLAMP (p0->col.ch [0], 0, 255);
        p0->col.ch [1] += chadd;
        p0->col.ch [1] = CLAMP (p0->col.ch [1], 0, 255);
    }
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
rgba_to_internal_rgb (ChafaCanvas *canvas, const guint8 *data, gint width, gint height, gint rowstride)
{
    ChafaPixel *pixel;
    gint px, py;
    gint x_inc, y_inc;
    gint alpha_sum = 0;

    x_inc = (width * FIXED_MULT) / (canvas->width_pixels);
    y_inc = (height * FIXED_MULT) / (canvas->height_pixels);

    pixel = canvas->pixels;

    for (py = 0; py < canvas->height_pixels; py++)
    {
        const guint8 *data_row_p;

        data_row_p = data + ((py * y_inc) / FIXED_MULT) * rowstride;

        for (px = 0; px < canvas->width_pixels; px++)
        {
            const guint8 *data_p = data_row_p + ((px * x_inc) / FIXED_MULT) * 4;

            pixel->col.ch [0] = *data_p++;
            pixel->col.ch [1] = *data_p++;
            pixel->col.ch [2] = *data_p++;
            pixel->col.ch [3] = *data_p;

            alpha_sum += (0xff - pixel->col.ch [3]);

            pixel++;
        }
    }

    if (alpha_sum == 0)
    {
        canvas->have_alpha = FALSE;
    }
    else
    {
        canvas->have_alpha = TRUE;
        multiply_alpha (canvas);
    }
}

static void
rgba_to_internal_din99d (ChafaCanvas *canvas, const guint8 *data, gint width, gint height, gint rowstride)
{
    ChafaPixel *pixel;
    gint px, py;
    gint x_inc, y_inc;
    gint alpha_sum = 0;

    x_inc = (width * FIXED_MULT) / (canvas->width_pixels);
    y_inc = (height * FIXED_MULT) / (canvas->height_pixels);

    pixel = canvas->pixels;

    for (py = 0; py < canvas->height_pixels; py++)
    {
        const guint8 *data_row_p;

        data_row_p = data + ((py * y_inc) / FIXED_MULT) * rowstride;

        for (px = 0; px < canvas->width_pixels; px++)
        {
            const guint8 *data_p = data_row_p + ((px * x_inc) / FIXED_MULT) * 4;
            ChafaColor rgb_col;

            rgb_col.ch [0] = *data_p++;
            rgb_col.ch [1] = *data_p++;
            rgb_col.ch [2] = *data_p++;
            rgb_col.ch [3] = *data_p;

            alpha_sum += (0xff - rgb_col.ch [3]);

            chafa_color_rgb_to_din99d (&rgb_col, &pixel->col);

            pixel++;
        }
    }

    if (alpha_sum == 0)
    {
        canvas->have_alpha = FALSE;
    }
    else
    {
        canvas->have_alpha = TRUE;
        multiply_alpha (canvas);
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

    chafa_symbol_map_prepare (&canvas->config.symbol_map);

    /* In truecolor mode we don't support any fancy color spaces for now, since
     * we'd have to convert back to RGB space when emitting control codes, and
     * the code for that has yet to be written. In palette modes we just use
     * the palette mappings. */
    if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_TRUECOLOR)
        canvas->config.color_space = CHAFA_COLOR_SPACE_RGB;

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

    switch (canvas->config.color_space)
    {
        case CHAFA_COLOR_SPACE_RGB:
            rgba_to_internal_rgb (canvas, src_pixels, src_width, src_height, src_rowstride);
            if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG
                || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG_BGFG)
                adjust_levels_rgb (canvas);
            break;

        case CHAFA_COLOR_SPACE_DIN99D:
            rgba_to_internal_din99d (canvas, src_pixels, src_width, src_height, src_rowstride);
            if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG
                || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG_BGFG)
                adjust_levels_din99d (canvas);
            break;

        case CHAFA_COLOR_SPACE_MAX:
            g_assert_not_reached ();
            break;
    }

    if (canvas->config.alpha_threshold == 0)
        canvas->have_alpha = FALSE;

    update_cells (canvas);
    canvas->needs_clear = FALSE;

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
