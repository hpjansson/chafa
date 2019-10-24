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
#include "chafa.h"
#include "internal/chafa-private.h"
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
 * You can draw an image to the canvas using chafa_canvas_set_contents ()
 * and create an ANSI text representation of the canvas' current contents
 * using chafa_canvas_build_ansi ().
 **/

/**
 * ChafaPixelType:
 * @CHAFA_PIXEL_RGBA8_PREMULTIPLIED: Premultiplied RGBA, 8 bits per channel.
 * @CHAFA_PIXEL_BGRA8_PREMULTIPLIED: Premultiplied BGRA, 8 bits per channel.
 * @CHAFA_PIXEL_ARGB8_PREMULTIPLIED: Premultiplied ARGB, 8 bits per channel.
 * @CHAFA_PIXEL_ABGR8_PREMULTIPLIED: Premultiplied ABGR, 8 bits per channel.
 * @CHAFA_PIXEL_RGBA8_UNASSOCIATED: Unassociated RGBA, 8 bits per channel.
 * @CHAFA_PIXEL_BGRA8_UNASSOCIATED: Unassociated BGRA, 8 bits per channel.
 * @CHAFA_PIXEL_ARGB8_UNASSOCIATED: Unassociated ARGB, 8 bits per channel.
 * @CHAFA_PIXEL_ABGR8_UNASSOCIATED: Unassociated ABGR, 8 bits per channel.
 * @CHAFA_PIXEL_RGB8: Packed RGB (no alpha), 8 bits per channel.
 * @CHAFA_PIXEL_BGR8: Packed BGR (no alpha), 8 bits per channel.
 * @CHAFA_PIXEL_MAX: Last supported pixel type, plus one.
 *
 * Pixel formats supported by #ChafaCanvas.
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
    ChafaPixelType src_pixel_type;
    gint src_width, src_height, src_rowstride;
    gint have_alpha_int;
    gint dither_grain_width_shift, dither_grain_height_shift;

    /* Set if we're doing bayer dithering */
    gint *bayer_matrix;
    gint bayer_size_shift;

    /* Set if we're in sixel mode */
    ChafaSixelCanvas *sixel_canvas;
};

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

static void
color_to_accum (const ChafaColor *color, ChafaColorAccum *accum)
{
    gint i;

    for (i = 0; i < 4; i++)
        accum->ch [i] = color->ch [i];
}

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
pick_symbol_and_colors_slow (ChafaCanvas *canvas,
                             WorkCell *wcell,
                             gunichar *sym_out,
                             ChafaColor *fg_col_out,
                             ChafaColor *bg_col_out,
                             gint *error_out)
{
    SymbolEval *eval;
    gint n;
    gint i;

    eval = g_alloca (sizeof (SymbolEval) * (canvas->config.symbol_map.n_symbols + 1));
    memset (eval, 0, sizeof (SymbolEval) * (canvas->config.symbol_map.n_symbols + 1));

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

            eval_symbol_colors (canvas, wcell, &canvas->config.symbol_map.symbols [i], &eval [i]);

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

        eval_symbol_error (canvas, wcell, &canvas->config.symbol_map.symbols [i], &eval [i]);
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

    if (n_candidates == 1)
    {
        if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG)
        {
            eval [0].fg.col = canvas->fg_color;
            eval [0].bg.col = canvas->bg_color;
        }
        else
        {
            eval_symbol_colors (canvas, wcell,
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
                eval_symbol_colors (canvas, wcell, sym, &eval [i]);
            }

            eval_symbol_error (canvas, wcell, sym, &eval [i]);

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
        ChafaCanvasCell *cell = &canvas->cells [i];
        WorkCell wcell;
        gunichar sym = 0;
        ChafaColorCandidates ccand;
        ChafaColor fg_col, bg_col;

        memset (cell, 0, sizeof (*cell));
        cell->c = ' ';

        work_cell_init (&wcell, canvas, cx, cy);

        if (canvas->config.symbol_map.n_symbols > 0)
        {
            if (canvas->work_factor_int >= 8)
                pick_symbol_and_colors_slow (canvas, &wcell, &sym, &fg_col, &bg_col, NULL);
            else
                pick_symbol_and_colors_fast (canvas, &wcell, &sym, &fg_col, &bg_col, NULL);

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
            apply_fill (canvas, &wcell, cell);
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
normalize_ch (guint8 v, gint min, gint factor)
{
    gint vt = v;

    vt -= min;
    vt *= factor;
    vt /= FIXED_MULT;

    vt = CLAMP (vt, 0, 255);
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
    gint ch [3];

#define Pr .299
#define Pg .587
#define Pb .144
    gdouble P = sqrt ((col->ch [0]) * (gdouble) (col->ch [0]) * Pr
                      + (col->ch [1]) * (gdouble) (col->ch [1]) * Pg
                      + (col->ch [2]) * (gdouble) (col->ch [2]) * Pb);

    ch [0] = P + ((gdouble) col->ch [0] - P) * 2;
    ch [1] = P + ((gdouble) col->ch [1] - P) * 2;
    ch [2] = P + ((gdouble) col->ch [2] - P) * 2;

    col->ch [0] = CLAMP (ch [0], 0, 255);
    col->ch [1] = CLAMP (ch [1], 0, 255);
    col->ch [2] = CLAMP (ch [2], 0, 255);
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
    gint16 bayer_mod = matrix [bayer_index];
    gint i;

    for (i = 0; i < 4; i++)
    {
        gint16 c;

        c = (gint16) pixel->col.ch [i] + bayer_mod;
        c = CLAMP (c, 0, 255);
        pixel->col.ch [i] = c;
    }
}

/* pixel must point to top-left pixel of the grain to be dithered */
static void
fs_dither_grain (ChafaCanvas *canvas, ChafaPixel *pixel, const ChafaColorAccum *error_in,
                 ChafaColorAccum *error_out_0, ChafaColorAccum *error_out_1,
                 ChafaColorAccum *error_out_2, ChafaColorAccum *error_out_3)
{
    gint grain_shift = canvas->dither_grain_width_shift + canvas->dither_grain_height_shift;
    ChafaColorAccum next_error = { 0 };
    ChafaColorAccum accum = { 0 };
    ChafaColorCandidates cand = { 0 };
    ChafaPixel *p;
    ChafaColor acol;
    const ChafaColor *col;
    gint x, y, i;

    p = pixel;

    for (y = 0; y < canvas->config.dither_grain_height; y++)
    {
        for (x = 0; x < canvas->config.dither_grain_width; x++, p++)
        {
            for (i = 0; i < 3; i++)
            {
                gint16 ch = p->col.ch [i];
                ch += error_in->ch [i];

                if (ch < 0)
                {
                    next_error.ch [i] += ch;
                    ch = 0;
                }
                else if (ch > 255)
                {
                    next_error.ch [i] += ch - 255;
                    ch = 255;
                }

                p->col.ch [i] = ch;
                accum.ch [i] += ch;
            }
        }

        p += canvas->width_pixels - canvas->config.dither_grain_width;
    }

    for (i = 0; i < 3; i++)
    {
        accum.ch [i] >>= grain_shift;
        acol.ch [i] = accum.ch [i];
    }

    /* Don't try to dither alpha */
    acol.ch [3] = 0xff;

    if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_256)
        chafa_pick_color_256 (&acol, canvas->config.color_space, &cand);
    else if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_240)
        chafa_pick_color_240 (&acol, canvas->config.color_space, &cand);
    else if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_16)
        chafa_pick_color_16 (&acol, canvas->config.color_space, &cand);
    else
        chafa_pick_color_fgbg (&acol, canvas->config.color_space,
                               &canvas->fg_color, &canvas->bg_color, &cand);

    col = chafa_get_palette_color_256 (cand.index [0], canvas->config.color_space);

    for (i = 0; i < 3; i++)
    {
        /* FIXME: Floating point op is slow. Factor this out and make
         * dither_intensity == 1.0 the fast path. */
        next_error.ch [i] = ((next_error.ch [i] >> grain_shift) + (accum.ch [i] - (gint16) col->ch [i]) * canvas->config.dither_intensity);

        error_out_0->ch [i] += next_error.ch [i] * 7 / 16;
        error_out_1->ch [i] += next_error.ch [i] * 1 / 16;
        error_out_2->ch [i] += next_error.ch [i] * 5 / 16;
        error_out_3->ch [i] += next_error.ch [i] * 3 / 16;
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
    ChafaColorAccum *error_rows;
    ChafaColorAccum *error_row [2];
    ChafaColorAccum *pp;
    gint width_grains = canvas->width_pixels >> canvas->dither_grain_width_shift;
    gint x, y;

    g_assert (canvas->width_pixels % canvas->config.dither_grain_width == 0);
    g_assert (dest_y % canvas->config.dither_grain_height == 0);
    g_assert (n_rows % canvas->config.dither_grain_height == 0);

    dest_y >>= canvas->dither_grain_height_shift;
    n_rows >>= canvas->dither_grain_height_shift;

    error_rows = alloca (width_grains * 2 * sizeof (ChafaColorAccum));
    error_row [0] = error_rows;
    error_row [1] = error_rows + width_grains;

    memset (error_row [0], 0, width_grains * sizeof (ChafaColorAccum));

    for (y = dest_y; y < dest_y + n_rows; y++)
    {
        memset (error_row [1], 0, width_grains * sizeof (ChafaColorAccum));

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
    SmolScaleCtx *scale_ctx;
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
prepare_pixels_1_inner (PreparePixelsBatch1 *work,
                        PrepareContext *prep_ctx,
                        const guint8 *data_p,
                        ChafaPixel *pixel_out,
                        gint *alpha_sum)
{
    ChafaColor *col = &pixel_out->col;
    gint v;

    col->ch [0] = data_p [0];
    col->ch [1] = data_p [1];
    col->ch [2] = data_p [2];
    col->ch [3] = data_p [3];

    *alpha_sum += (0xff - col->ch [3]);

    if (prep_ctx->canvas->config.preprocessing_enabled
        && prep_ctx->canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_16)
    {
        boost_saturation_rgb (col);
    }

    /* Build histogram */
    v = rgb_to_intensity_fast (col);
    work->hist.c [v]++;
}

static void
prepare_pixels_1_worker_nearest (PreparePixelsBatch1 *work, PrepareContext *prep_ctx)
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
            prepare_pixels_1_inner (work, prep_ctx, data_p, pixel++, &alpha_sum);
        }
    }

    if (alpha_sum > 0)
        g_atomic_int_set (&canvas->have_alpha_int, 1);
}

static void
prepare_pixels_1_worker_smooth (PreparePixelsBatch1 *work, PrepareContext *prep_ctx)
{
    ChafaCanvas *canvas = prep_ctx->canvas;
    ChafaPixel *pixel, *pixel_max;
    gint alpha_sum = 0;
    guint8 *scaled_data;
    const guint8 *data_p;

    canvas = prep_ctx->canvas;

    scaled_data = g_malloc (canvas->width_pixels * work->n_rows * sizeof (guint32));
    smol_scale_batch_full (prep_ctx->scale_ctx, scaled_data, work->first_row, work->n_rows);

    data_p = scaled_data;
    pixel = canvas->pixels + work->first_row * canvas->width_pixels;
    pixel_max = pixel + work->n_rows * canvas->width_pixels;

    while (pixel < pixel_max)
    {
        prepare_pixels_1_inner (work, prep_ctx, data_p, pixel++, &alpha_sum);
        data_p += 4;
    }

    g_free (scaled_data);

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
     * - Figure out if we have alpha transparency
     */

    batches = g_new0 (PreparePixelsBatch1, prep_ctx->n_batches_pixels);

    thread_pool = g_thread_pool_new ((GFunc) ((prep_ctx->canvas->work_factor_int < 3
                                               && prep_ctx->canvas->src_pixel_type == CHAFA_PIXEL_RGBA8_UNASSOCIATED)
                                              ? prepare_pixels_1_worker_nearest
                                              : prepare_pixels_1_worker_smooth),
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

    /* Report alpha situation */
    if (prep_ctx->canvas->have_alpha_int)
        prep_ctx->canvas->have_alpha = TRUE;

    g_free (batches);
}

typedef struct
{
    gint first_row;
    gint n_rows;
}
PreparePixelsBatch2;

static void
composite_alpha_on_bg (ChafaCanvas *canvas, gint first_row, gint n_rows)
{
    ChafaPixel *p0, *p1;

    p0 = canvas->pixels + first_row * canvas->width_pixels;
    p1 = p0 + n_rows * canvas->width_pixels;

    for ( ; p0 < p1; p0++)
    {
        p0->col.ch [0] += (canvas->bg_color.ch [0] * (255 - (guint32) p0->col.ch [3])) / 255;
        p0->col.ch [1] += (canvas->bg_color.ch [1] * (255 - (guint32) p0->col.ch [3])) / 255;
        p0->col.ch [2] += (canvas->bg_color.ch [2] * (255 - (guint32) p0->col.ch [3])) / 255;
    }
}

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

    /* Must do this after DIN99d conversion, since bg_color will be DIN99d too */
    if (canvas->have_alpha)
        composite_alpha_on_bg (canvas, work->first_row, work->n_rows);
}

static gboolean
need_pass_2 (ChafaCanvas *canvas)
{
    if ((canvas->config.preprocessing_enabled
         && (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_16
             || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG_BGFG
             || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG))
        || canvas->have_alpha
        || canvas->config.color_space == CHAFA_COLOR_SPACE_DIN99D
        || canvas->config.dither_mode != CHAFA_DITHER_MODE_NONE)
        return TRUE;

    return FALSE;
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

    if (!need_pass_2 (prep_ctx->canvas))
        return;

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

    prep_ctx.scale_ctx = smol_scale_new ((SmolPixelType) canvas->src_pixel_type,
                                         (const guint32 *) canvas->src_pixels,
                                         canvas->src_width,
                                         canvas->src_height,
                                         canvas->src_rowstride,
                                         SMOL_PIXEL_RGBA8_PREMULTIPLIED,
                                         NULL,
                                         canvas->width_pixels,
                                         canvas->height_pixels,
                                         canvas->width_pixels * sizeof (guint32));

    prepare_pixels_pass_1 (&prep_ctx);
    prepare_pixels_pass_2 (&prep_ctx);

    smol_scale_destroy (prep_ctx.scale_ctx);
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
        canvas->hist = g_new (Histogram, 1);

        canvas->src_pixel_type = src_pixel_type;
        canvas->src_pixels = src_pixels;
        canvas->src_width = src_width;
        canvas->src_height = src_height;
        canvas->src_rowstride = src_rowstride;
        canvas->have_alpha_int = 0;

        prepare_pixel_data (canvas);

        if (canvas->config.alpha_threshold == 0)
            canvas->have_alpha = FALSE;

        update_cells (canvas);
        canvas->needs_clear = FALSE;

        g_free (canvas->pixels);
        canvas->pixels = NULL;

        g_free (canvas->hist);
        canvas->hist = NULL;
    }
    else
    {
        /* Sixel mode */

        canvas->sixel_canvas = chafa_sixel_canvas_new (canvas->width_pixels,
                                                       canvas->height_pixels,
                                                       canvas->config.color_space,
                                                       canvas->config.alpha_threshold);
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
        str = build_ansi_gstring (canvas);
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
