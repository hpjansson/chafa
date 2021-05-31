/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2020-2021 Hans Petter Jansson
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

#include "internal/chafa-pixops.h"
#include "internal/smolscale/smolscale.h"

/* Fixed point multiplier */
#define FIXED_MULT 16384

/* See rgb_to_intensity_fast () */
#define INTENSITY_MAX (256 * 8)

/* Normalization: Percentage of pixels to discard at extremes of histogram */
#define INDEXED_16_CROP_PCT 5
#define INDEXED_8_CROP_PCT  10
#define INDEXED_2_CROP_PCT  20

typedef struct
{
    gint32 c [INTENSITY_MAX];

    /* Transparent pixels are not sampled, so we must keep count */
    gint n_samples;

    /* Lower and upper bounds */
    gint32 min, max;
}
Histogram;

typedef struct
{
    ChafaPixelType src_pixel_type;
    gconstpointer src_pixels;
    gint src_width, src_height;
    gint src_rowstride;

    ChafaPixel *dest_pixels;
    gint dest_width, dest_height;

    const ChafaPalette *palette;
    const ChafaDither *dither;
    ChafaColorSpace color_space;
    gboolean preprocessing_enabled;
    gint work_factor_int;

    /* Cached to avoid repeatedly calling palette functions */
    ChafaPaletteType palette_type;
    ChafaColor bg_color_rgb;

    /* Result of alpha detection is stored here */
    gint have_alpha_int;

    Histogram hist;
    gint n_batches_pixels;
    gint n_rows_per_batch_pixels;

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

typedef struct
{
    gint first_row;
    gint n_rows;
}
PreparePixelsBatch2;

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

    hist_accum->n_samples += hist_in->n_samples;

    for (i = 0; i < INTENSITY_MAX; i++)
    {
        hist_accum->c [i] += hist_in->c [i];
    }
}

static void
histogram_calc_bounds (Histogram *hist, gint crop_pct)
{
    gint64 pixels_crop;
    gint i;
    gint t;

    pixels_crop = (hist->n_samples * (((gint64) crop_pct * 1024) / 100)) / 1024;

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
normalize_rgb (ChafaPixel *pixels, const Histogram *hist, gint width, gint dest_y, gint n_rows)
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

    p0 = pixels + dest_y * width;
    p1 = p0 + n_rows * width;

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
    gfloat P = sqrtf (col->ch [0] * (gfloat) col->ch [0] * .299f
                      + col->ch [1] * (gfloat) col->ch [1] * .587f
                      + col->ch [2] * (gfloat) col->ch [2] * .144f);

    ch [0] = P + ((gfloat) col->ch [0] - P) * 2;
    ch [1] = P + ((gfloat) col->ch [1] - P) * 2;
    ch [2] = P + ((gfloat) col->ch [2] - P) * 2;

    col->ch [0] = CLAMP (ch [0], 0, 255);
    col->ch [1] = CLAMP (ch [1], 0, 255);
    col->ch [2] = CLAMP (ch [2], 0, 255);
}

/* pixel must point to top-left pixel of the grain to be dithered */
static void
fs_dither_grain (const ChafaDither *dither,
                 const ChafaPalette *palette, ChafaColorSpace color_space,
                 ChafaPixel *pixel, gint image_width,
                 const ChafaColorAccum *error_in,
                 ChafaColorAccum *error_out_0, ChafaColorAccum *error_out_1,
                 ChafaColorAccum *error_out_2, ChafaColorAccum *error_out_3)
{
    gint grain_width = 1 << dither->grain_width_shift;
    gint grain_height = 1 << dither->grain_height_shift;
    gint grain_shift = dither->grain_width_shift + dither->grain_height_shift;
    ChafaColorAccum next_error = { 0 };
    ChafaColorAccum accum = { 0 };
    ChafaColorCandidates cand = { 0 };
    ChafaPixel *p;
    ChafaColor acol;
    const ChafaColor *col;
    gint x, y, i;

    p = pixel;

    for (y = 0; y < grain_height; y++)
    {
        for (x = 0; x < grain_width; x++, p++)
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

        p += image_width - grain_width;
    }

    for (i = 0; i < 3; i++)
    {
        accum.ch [i] >>= grain_shift;
        acol.ch [i] = accum.ch [i];
    }

    /* Don't try to dither alpha */
    acol.ch [3] = 0xff;

    chafa_palette_lookup_nearest (palette, color_space, &acol, &cand);
    col = chafa_palette_get_color (palette, color_space, cand.index [0]);

    for (i = 0; i < 3; i++)
    {
        /* FIXME: Floating point op is slow. Factor this out and make
         * dither_intensity == 1.0 the fast path. */
        next_error.ch [i] = ((next_error.ch [i] >> grain_shift) + (accum.ch [i] - (gint16) col->ch [i]) * dither->intensity);

        error_out_0->ch [i] += next_error.ch [i] * 7 / 16;
        error_out_1->ch [i] += next_error.ch [i] * 1 / 16;
        error_out_2->ch [i] += next_error.ch [i] * 5 / 16;
        error_out_3->ch [i] += next_error.ch [i] * 3 / 16;
    }
}

static void
convert_rgb_to_din99d (ChafaPixel *pixels, gint width, gint dest_y, gint n_rows)
{
    ChafaPixel *pixel = pixels + dest_y * width;
    ChafaPixel *pixel_max = pixel + n_rows * width;

    /* RGB -> DIN99d */

    for ( ; pixel < pixel_max; pixel++)
    {
        chafa_color_rgb_to_din99d (&pixel->col, &pixel->col);
    }
}

static void
bayer_dither (const ChafaDither *dither, ChafaPixel *pixels, gint width, gint dest_y, gint n_rows)
{
    ChafaPixel *pixel = pixels + dest_y * width;
    ChafaPixel *pixel_max = pixel + n_rows * width;
    gint x, y;

    for (y = dest_y; pixel < pixel_max; y++)
    {
        for (x = 0; x < width; x++)
        {
            pixel->col = chafa_dither_color_ordered (dither, pixel->col, x, y);
            pixel++;
        }
    }
}

static void
fs_dither (const ChafaDither *dither, const ChafaPalette *palette,
           ChafaColorSpace color_space,
           ChafaPixel *pixels, gint width, gint dest_y, gint n_rows)
{
    ChafaPixel *pixel;
    ChafaColorAccum *error_rows;
    ChafaColorAccum *error_row [2];
    ChafaColorAccum *pp;
    gint grain_width = 1 << dither->grain_width_shift;
    gint grain_height = 1 << dither->grain_height_shift;
    gint width_grains = width >> dither->grain_width_shift;
    gint x, y;

    g_assert (width % grain_width == 0);
    g_assert (dest_y % grain_height == 0);
    g_assert (n_rows % grain_height == 0);

    dest_y >>= dither->grain_height_shift;
    n_rows >>= dither->grain_height_shift;

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
            pixel = pixels + (y << dither->grain_height_shift) * width;

            fs_dither_grain (dither, palette, color_space, pixel, width,
                             error_row [0],
                             error_row [0] + 1,
                             error_row [1] + 1,
                             error_row [1],
                             error_row [1] + 1);
            pixel += grain_width;

            for (x = 1; ((x + 1) << dither->grain_width_shift) < width; x++)
            {
                fs_dither_grain (dither, palette, color_space, pixel, width,
                                 error_row [0] + x,
                                 error_row [0] + x + 1,
                                 error_row [1] + x + 1,
                                 error_row [1] + x,
                                 error_row [1] + x - 1);
                pixel += grain_width;
            }

            fs_dither_grain (dither, palette, color_space, pixel, width,
                             error_row [0] + x,
                             error_row [1] + x,
                             error_row [1] + x,
                             error_row [1] + x - 1,
                             error_row [1] + x - 1);
        }
        else
        {
            /* Backwards pass */
            pixel = pixels + (y << dither->grain_height_shift) * (width + 1) - grain_width;

            fs_dither_grain (dither, palette, color_space, pixel, width,
                             error_row [0] + width_grains - 1,
                             error_row [0] + width_grains - 2,
                             error_row [1] + width_grains - 2,
                             error_row [1] + width_grains - 1,
                             error_row [1] + width_grains - 2);

            pixel -= grain_width;

            for (x = width_grains - 2; x > 0; x--)
            {
                fs_dither_grain (dither, palette, color_space, pixel, width,
                                 error_row [0] + x,
                                 error_row [0] + x - 1,
                                 error_row [1] + x - 1,
                                 error_row [1] + x,
                                 error_row [1] + x + 1);
                pixel -= grain_width;
            }

            fs_dither_grain (dither, palette, color_space, pixel, width,
                             error_row [0],
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
bayer_and_convert_rgb_to_din99d (const ChafaDither *dither,
                                 ChafaPixel *pixels, gint width, gint dest_y, gint n_rows)
{
    ChafaPixel *pixel = pixels + dest_y * width;
    ChafaPixel *pixel_max = pixel + n_rows * width;
    gint x, y;

    for (y = dest_y; pixel < pixel_max; y++)
    {
        for (x = 0; x < width; x++)
        {
            pixel->col = chafa_dither_color_ordered (dither, pixel->col, x, y);
            chafa_color_rgb_to_din99d (&pixel->col, &pixel->col);
            pixel++;
        }
    }
}

static void
fs_and_convert_rgb_to_din99d (const ChafaDither *dither, const ChafaPalette *palette,
                              ChafaPixel *pixels, gint width, gint dest_y, gint n_rows)
{
    convert_rgb_to_din99d (pixels, width, dest_y, n_rows);
    fs_dither (dither, palette, CHAFA_COLOR_SPACE_DIN99D, pixels, width, dest_y, n_rows);
}

static void
prepare_pixels_1_inner (PreparePixelsBatch1 *work,
                        PrepareContext *prep_ctx,
                        const guint8 *data_p,
                        ChafaPixel *pixel_out,
                        gint *alpha_sum)
{
    ChafaColor *col = &pixel_out->col;

    col->ch [0] = data_p [0];
    col->ch [1] = data_p [1];
    col->ch [2] = data_p [2];
    col->ch [3] = data_p [3];

    *alpha_sum += (0xff - col->ch [3]);

    if (prep_ctx->preprocessing_enabled
        && (prep_ctx->palette_type == CHAFA_PALETTE_TYPE_FIXED_16
            || prep_ctx->palette_type == CHAFA_PALETTE_TYPE_FIXED_8)
        )
    {
        boost_saturation_rgb (col);
    }

    /* Build histogram */
    if (col->ch [3] > 127)
    {
        gint v = rgb_to_intensity_fast (col);
        work->hist.c [v]++;
        work->hist.n_samples++;
    }
}

static void
prepare_pixels_1_worker_nearest (PreparePixelsBatch1 *work, PrepareContext *prep_ctx)
{
    ChafaPixel *pixel;
    gint dest_y;
    gint px, py;
    gint x_inc, y_inc;
    gint alpha_sum = 0;
    const guint8 *data;
    gint n_rows;
    gint rowstride;

    dest_y = work->first_row;
    data = prep_ctx->src_pixels;
    n_rows = work->n_rows;
    rowstride = prep_ctx->src_rowstride;

    x_inc = (prep_ctx->src_width * FIXED_MULT) / (prep_ctx->dest_width);
    y_inc = (prep_ctx->src_height * FIXED_MULT) / (prep_ctx->dest_height);

    pixel = prep_ctx->dest_pixels + dest_y * prep_ctx->dest_width;

    for (py = dest_y; py < dest_y + n_rows; py++)
    {
        const guint8 *data_row_p;

        data_row_p = data + ((py * y_inc) / FIXED_MULT) * rowstride;

        for (px = 0; px < prep_ctx->dest_width; px++)
        {
            const guint8 *data_p = data_row_p + ((px * x_inc) / FIXED_MULT) * 4;
            prepare_pixels_1_inner (work, prep_ctx, data_p, pixel++, &alpha_sum);
        }
    }

    if (alpha_sum > 0)
        g_atomic_int_set (&prep_ctx->have_alpha_int, 1);
}

static void
prepare_pixels_1_worker_smooth (PreparePixelsBatch1 *work, PrepareContext *prep_ctx)
{
    ChafaPixel *pixel, *pixel_max;
    gint alpha_sum = 0;
    guint8 *scaled_data;
    const guint8 *data_p;

    scaled_data = g_malloc (prep_ctx->dest_width * work->n_rows * sizeof (guint32));
    smol_scale_batch_full (prep_ctx->scale_ctx, scaled_data, work->first_row, work->n_rows);

    data_p = scaled_data;
    pixel = prep_ctx->dest_pixels + work->first_row * prep_ctx->dest_width;
    pixel_max = pixel + work->n_rows * prep_ctx->dest_width;

    while (pixel < pixel_max)
    {
        prepare_pixels_1_inner (work, prep_ctx, data_p, pixel++, &alpha_sum);
        data_p += 4;
    }

    g_free (scaled_data);

    if (alpha_sum > 0)
        g_atomic_int_set (&prep_ctx->have_alpha_int, 1);
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

    thread_pool = g_thread_pool_new ((GFunc) ((prep_ctx->work_factor_int < 3
                                               && prep_ctx->src_pixel_type == CHAFA_PIXEL_RGBA8_UNASSOCIATED)
                                              ? prepare_pixels_1_worker_nearest
                                              : prepare_pixels_1_worker_smooth),
                                     prep_ctx,
                                     g_get_num_processors (),
                                     FALSE,
                                     NULL);

    for (cy = 0, i = 0;
         cy < prep_ctx->dest_height;
         cy += prep_ctx->n_rows_per_batch_pixels, i++)
    {
        PreparePixelsBatch1 *batch = &batches [i];

        batch->first_row = cy;
        batch->n_rows = MIN (prep_ctx->dest_height - cy, prep_ctx->n_rows_per_batch_pixels);

        g_thread_pool_push (thread_pool, batch, NULL);
    }

    /* Wait for threads to finish */
    g_thread_pool_free (thread_pool, FALSE, TRUE);

    /* Generate final histogram */
    if (prep_ctx->preprocessing_enabled)
    {
        for (i = 0; i < prep_ctx->n_batches_pixels; i++)
            sum_histograms (&batches [i].hist, &prep_ctx->hist);

        switch (prep_ctx->palette_type)
        {
          case CHAFA_PALETTE_TYPE_FIXED_16:
            histogram_calc_bounds (&prep_ctx->hist, INDEXED_16_CROP_PCT);
            break;
          case CHAFA_PALETTE_TYPE_FIXED_8:
            histogram_calc_bounds (&prep_ctx->hist, INDEXED_8_CROP_PCT);
            break;
          default:
            histogram_calc_bounds (&prep_ctx->hist, INDEXED_2_CROP_PCT);
            break;
        }
    }

    g_free (batches);
}

static void
composite_alpha_on_bg (ChafaColor bg_color,
                       ChafaPixel *pixels, gint width, gint first_row, gint n_rows)
{
    ChafaPixel *p0, *p1;

    p0 = pixels + first_row * width;
    p1 = p0 + n_rows * width;

    for ( ; p0 < p1; p0++)
    {
        p0->col.ch [0] += (bg_color.ch [0] * (255 - (guint32) p0->col.ch [3])) / 255;
        p0->col.ch [1] += (bg_color.ch [1] * (255 - (guint32) p0->col.ch [3])) / 255;
        p0->col.ch [2] += (bg_color.ch [2] * (255 - (guint32) p0->col.ch [3])) / 255;
    }
}

static void
prepare_pixels_2_worker (PreparePixelsBatch2 *work, PrepareContext *prep_ctx)
{
    if (prep_ctx->preprocessing_enabled
        && (prep_ctx->palette_type == CHAFA_PALETTE_TYPE_FIXED_16
            ||prep_ctx->palette_type == CHAFA_PALETTE_TYPE_FIXED_8
            || prep_ctx->palette_type == CHAFA_PALETTE_TYPE_FIXED_FGBG))
        normalize_rgb (prep_ctx->dest_pixels, &prep_ctx->hist, prep_ctx->dest_width,
                       work->first_row, work->n_rows);

    if (prep_ctx->have_alpha_int)
        composite_alpha_on_bg (prep_ctx->bg_color_rgb,
                               prep_ctx->dest_pixels, prep_ctx->dest_width,
                               work->first_row, work->n_rows);

    if (prep_ctx->color_space == CHAFA_COLOR_SPACE_DIN99D)
    {
        if (prep_ctx->dither->mode == CHAFA_DITHER_MODE_ORDERED)
        {
            bayer_and_convert_rgb_to_din99d (prep_ctx->dither,
                                             prep_ctx->dest_pixels,
                                             prep_ctx->dest_width,
                                             work->first_row,
                                             work->n_rows);
        }
        else if (prep_ctx->dither->mode == CHAFA_DITHER_MODE_DIFFUSION)
        {
            fs_and_convert_rgb_to_din99d (prep_ctx->dither,
                                          prep_ctx->palette,
                                          prep_ctx->dest_pixels,
                                          prep_ctx->dest_width,
                                          work->first_row,
                                          work->n_rows);
        }
        else
        {
            convert_rgb_to_din99d (prep_ctx->dest_pixels,
                                   prep_ctx->dest_width,
                                   work->first_row,
                                   work->n_rows);
        }
    }
    else if (prep_ctx->dither->mode == CHAFA_DITHER_MODE_ORDERED)
    {
        bayer_dither (prep_ctx->dither,
                      prep_ctx->dest_pixels,
                      prep_ctx->dest_width,
                      work->first_row,
                      work->n_rows);
    }
    else if (prep_ctx->dither->mode == CHAFA_DITHER_MODE_DIFFUSION)
    {
        fs_dither (prep_ctx->dither,
                   prep_ctx->palette,
                   prep_ctx->color_space,
                   prep_ctx->dest_pixels,
                   prep_ctx->dest_width,
                   work->first_row,
                   work->n_rows);
    }
}

static gboolean
need_pass_2 (PrepareContext *prep_ctx)
{
    if ((prep_ctx->preprocessing_enabled
         && (prep_ctx->palette_type == CHAFA_PALETTE_TYPE_FIXED_16
             || prep_ctx->palette_type == CHAFA_PALETTE_TYPE_FIXED_8
             || prep_ctx->palette_type == CHAFA_PALETTE_TYPE_FIXED_FGBG))
        || prep_ctx->have_alpha_int
        || prep_ctx->color_space == CHAFA_COLOR_SPACE_DIN99D
        || prep_ctx->dither->mode != CHAFA_DITHER_MODE_NONE)
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

    if (!need_pass_2 (prep_ctx))
        return;

    batches = g_new0 (PreparePixelsBatch1, prep_ctx->n_batches_pixels);

    thread_pool = g_thread_pool_new ((GFunc) prepare_pixels_2_worker,
                                     prep_ctx,
                                     g_get_num_processors (),
                                     FALSE,
                                     NULL);

    for (cy = 0, i = 0;
         cy < prep_ctx->dest_height;
         cy += prep_ctx->n_rows_per_batch_pixels, i++)
    {
        PreparePixelsBatch1 *batch = &batches [i];

        batch->first_row = cy;
        batch->n_rows = MIN (prep_ctx->dest_height - cy, prep_ctx->n_rows_per_batch_pixels);

        g_thread_pool_push (thread_pool, batch, NULL);
    }

    /* Wait for threads to finish */
    g_thread_pool_free (thread_pool, FALSE, TRUE);

    g_free (batches);
}

void
chafa_prepare_pixel_data_for_symbols (const ChafaPalette *palette,
                                      const ChafaDither *dither,
                                      ChafaColorSpace color_space,
                                      gboolean preprocessing_enabled,
                                      gint work_factor,
                                      ChafaPixelType src_pixel_type,
                                      gconstpointer src_pixels,
                                      gint src_width,
                                      gint src_height,
                                      gint src_rowstride,
                                      ChafaPixel *dest_pixels,
                                      gint dest_width,
                                      gint dest_height)
{
    PrepareContext prep_ctx = { 0 };
    guint n_cpus;

    n_cpus = g_get_num_processors ();

    prep_ctx.palette = palette;
    prep_ctx.dither = dither;
    prep_ctx.color_space = color_space;
    prep_ctx.preprocessing_enabled = preprocessing_enabled;
    prep_ctx.work_factor_int = work_factor;

    prep_ctx.palette_type = chafa_palette_get_type (palette);
    prep_ctx.bg_color_rgb = *chafa_palette_get_color (palette,
                                                      CHAFA_COLOR_SPACE_RGB,
                                                      CHAFA_PALETTE_INDEX_BG);

    prep_ctx.src_pixel_type = src_pixel_type;
    prep_ctx.src_pixels = src_pixels;
    prep_ctx.src_width = src_width;
    prep_ctx.src_height = src_height;
    prep_ctx.src_rowstride = src_rowstride;

    prep_ctx.dest_pixels = dest_pixels;
    prep_ctx.dest_width = dest_width;
    prep_ctx.dest_height = dest_height;

    prep_ctx.n_batches_pixels = (prep_ctx.dest_height + n_cpus - 1) / n_cpus;
    prep_ctx.n_rows_per_batch_pixels = (prep_ctx.dest_height + prep_ctx.n_batches_pixels - 1) / prep_ctx.n_batches_pixels;

    prep_ctx.scale_ctx = smol_scale_new ((SmolPixelType) prep_ctx.src_pixel_type,
                                         (const guint32 *) prep_ctx.src_pixels,
                                         prep_ctx.src_width,
                                         prep_ctx.src_height,
                                         prep_ctx.src_rowstride,
                                         SMOL_PIXEL_RGBA8_PREMULTIPLIED,
                                         NULL,
                                         prep_ctx.dest_width,
                                         prep_ctx.dest_height,
                                         prep_ctx.dest_width * sizeof (guint32));

    prepare_pixels_pass_1 (&prep_ctx);
    prepare_pixels_pass_2 (&prep_ctx);

    smol_scale_destroy (prep_ctx.scale_ctx);
}

void
chafa_sort_pixel_index_by_channel (guint8 *index, const ChafaPixel *pixels, gint n_pixels, gint ch)
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
