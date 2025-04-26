/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2020-2024 Hans Petter Jansson
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

#include "chafa.h"
#include "internal/chafa-batch.h"
#include "internal/chafa-pixops.h"
#include "internal/chafa-math-util.h"
#include "internal/smolscale/smolscale.h"

/* Fixed point multiplier */
#define FIXED_MULT 4096

/* See rgb_to_intensity_fast () */
#define INTENSITY_MAX (256 * 8)

/* Normalization: Percentage of pixels to discard at extremes of histogram */
#define INDEXED_16_CROP_PCT 5
#define INDEXED_8_CROP_PCT  10
#define INDEXED_2_CROP_PCT  20

/* Ensure there's no overflow in normalize_ch() */
G_STATIC_ASSERT (FIXED_MULT * INTENSITY_MAX * (gint64) 255 <= G_MAXINT);
G_STATIC_ASSERT (FIXED_MULT * INTENSITY_MAX * (gint64) -255 >= G_MININT);

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
    SmolScaleCtx *scale_ctx;
}
PrepareContext;

typedef struct
{
    Histogram hist;
}
PreparePixelsBatch1Ret;

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
    ChafaPixel *p;
    ChafaColor acol;
    const ChafaColor *col;
    gint index;
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

    index = chafa_palette_lookup_nearest (palette, color_space, &acol, NULL);
    col = chafa_palette_get_color (palette, color_space, index);

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
simple_dither (const ChafaDither *dither, ChafaPixel *pixels, gint width, gint dest_y, gint n_rows)
{
    ChafaPixel *pixel = pixels + dest_y * width;
    ChafaPixel *pixel_max = pixel + n_rows * width;
    gint x, y;

    for (y = dest_y; pixel < pixel_max; y++)
    {
        for (x = 0; x < width; x++)
        {
            pixel->col = chafa_dither_color (dither, pixel->col, x, y);
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

    error_rows = g_malloc (width_grains * 2 * sizeof (ChafaColorAccum));
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
            pixel = pixels + (y << dither->grain_height_shift) * width + width - grain_width;

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

    g_free (error_rows);
}

static void
dither_and_convert_rgb_to_din99d (const ChafaDither *dither,
                                  ChafaPixel *pixels, gint width, gint dest_y, gint n_rows)
{
    ChafaPixel *pixel = pixels + dest_y * width;
    ChafaPixel *pixel_max = pixel + n_rows * width;
    gint x, y;

    for (y = dest_y; pixel < pixel_max; y++)
    {
        for (x = 0; x < width; x++)
        {
            pixel->col = chafa_dither_color (dither, pixel->col, x, y);
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
prepare_pixels_1_inner (PreparePixelsBatch1Ret *ret,
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
        ret->hist.c [v]++;
        ret->hist.n_samples++;
    }
}

static void
prepare_pixels_1_worker_nearest (ChafaBatchInfo *batch, PrepareContext *prep_ctx)
{
    ChafaPixel *pixel;
    gint dest_y;
    gint px, py;
    gint x_inc, y_inc;
    gint alpha_sum = 0;
    const guint8 *data;
    gint n_rows;
    gint rowstride;
    PreparePixelsBatch1Ret *ret;

    ret = g_new (PreparePixelsBatch1Ret, 1);
    batch->ret_p = ret;

    dest_y = batch->first_row;
    data = prep_ctx->src_pixels;
    n_rows = batch->n_rows;
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
            prepare_pixels_1_inner (ret, prep_ctx, data_p, pixel++, &alpha_sum);
        }
    }

    if (alpha_sum > 0)
        g_atomic_int_set (&prep_ctx->have_alpha_int, 1);
}

static void
prepare_pixels_1_worker_smooth (ChafaBatchInfo *batch, PrepareContext *prep_ctx)
{
    ChafaPixel *pixel, *pixel_max;
    gint alpha_sum = 0;
    guint8 *scaled_data;
    const guint8 *data_p;
    PreparePixelsBatch1Ret *ret;

    ret = g_new0 (PreparePixelsBatch1Ret, 1);
    batch->ret_p = ret;

    scaled_data = g_malloc (prep_ctx->dest_width * batch->n_rows * sizeof (guint32));
    smol_scale_batch_full (prep_ctx->scale_ctx, scaled_data, batch->first_row, batch->n_rows);

    data_p = scaled_data;
    pixel = prep_ctx->dest_pixels + batch->first_row * prep_ctx->dest_width;
    pixel_max = pixel + batch->n_rows * prep_ctx->dest_width;

    while (pixel < pixel_max)
    {
        prepare_pixels_1_inner (ret, prep_ctx, data_p, pixel++, &alpha_sum);
        data_p += 4;
    }

    g_free (scaled_data);

    if (alpha_sum > 0)
        g_atomic_int_set (&prep_ctx->have_alpha_int, 1);
}

static void
pass_1_post (ChafaBatchInfo *batch, PrepareContext *prep_ctx)
{
    PreparePixelsBatch1Ret *ret = batch->ret_p;

    if (prep_ctx->preprocessing_enabled)
    {
        sum_histograms (&ret->hist, &prep_ctx->hist);
    }

    g_free (ret);
}

static void
prepare_pixels_pass_1 (PrepareContext *prep_ctx)
{
    GFunc batch_func;

    /* First pass
     * ----------
     *
     * - Scale and convert pixel format
     * - Apply local preprocessing like saturation boost (optional)
     * - Generate histogram for later passes (e.g. for normalization)
     * - Figure out if we have alpha transparency
     */

    batch_func = (GFunc) ((prep_ctx->work_factor_int < 3
                           && prep_ctx->src_pixel_type == CHAFA_PIXEL_RGBA8_UNASSOCIATED)
                          ? prepare_pixels_1_worker_nearest
                          : prepare_pixels_1_worker_smooth);

    chafa_process_batches (prep_ctx,
                           (GFunc) batch_func,
                           (GFunc) pass_1_post,
                           prep_ctx->dest_height,
                           chafa_get_n_actual_threads (),
                           1);

    /* Generate final histogram */
    if (prep_ctx->preprocessing_enabled)
    {
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
}

static void
composite_alpha_on_bg (ChafaColor bg_color,
                       ChafaPixel *pixels, gint width, gint first_row, gint n_rows)
{
    ChafaPixel *p0, *p1;

    p0 = pixels + first_row * width;
    p1 = p0 + n_rows * width;

    /* FIXME: This is slow and bad. We should fix it with a new Smolscale
     * compositing mode. */

    for ( ; p0 < p1; p0++)
    {
        p0->col.ch [0] = (p0->col.ch [0] * (guint32) p0->col.ch [3]
                          + bg_color.ch [0] * (255 - (guint32) p0->col.ch [3])) / 255;
        p0->col.ch [1] = (p0->col.ch [1] * (guint32) p0->col.ch [3]
                          + bg_color.ch [1] * (255 - (guint32) p0->col.ch [3])) / 255;
        p0->col.ch [2] = (p0->col.ch [2] * (guint32) p0->col.ch [3]
                          + bg_color.ch [2] * (255 - (guint32) p0->col.ch [3])) / 255;
    }
}

static void
prepare_pixels_2_worker (ChafaBatchInfo *batch, PrepareContext *prep_ctx)
{
    if (prep_ctx->preprocessing_enabled
        && (prep_ctx->palette_type == CHAFA_PALETTE_TYPE_FIXED_16
            || prep_ctx->palette_type == CHAFA_PALETTE_TYPE_FIXED_8
            || prep_ctx->palette_type == CHAFA_PALETTE_TYPE_FIXED_FGBG))
        normalize_rgb (prep_ctx->dest_pixels, &prep_ctx->hist, prep_ctx->dest_width,
                       batch->first_row, batch->n_rows);

    if (prep_ctx->have_alpha_int)
        composite_alpha_on_bg (prep_ctx->bg_color_rgb,
                               prep_ctx->dest_pixels, prep_ctx->dest_width,
                               batch->first_row, batch->n_rows);

    if (prep_ctx->color_space == CHAFA_COLOR_SPACE_DIN99D)
    {
        if (prep_ctx->dither->mode == CHAFA_DITHER_MODE_ORDERED
            || prep_ctx->dither->mode == CHAFA_DITHER_MODE_NOISE)
        {
            dither_and_convert_rgb_to_din99d (prep_ctx->dither,
                                              prep_ctx->dest_pixels,
                                              prep_ctx->dest_width,
                                              batch->first_row,
                                              batch->n_rows);
        }
        else if (prep_ctx->dither->mode == CHAFA_DITHER_MODE_DIFFUSION)
        {
            fs_and_convert_rgb_to_din99d (prep_ctx->dither,
                                          prep_ctx->palette,
                                          prep_ctx->dest_pixels,
                                          prep_ctx->dest_width,
                                          batch->first_row,
                                          batch->n_rows);
        }
        else
        {
            convert_rgb_to_din99d (prep_ctx->dest_pixels,
                                   prep_ctx->dest_width,
                                   batch->first_row,
                                   batch->n_rows);
        }
    }
    else if (prep_ctx->dither->mode == CHAFA_DITHER_MODE_ORDERED
             || prep_ctx->dither->mode == CHAFA_DITHER_MODE_NOISE)
    {
        simple_dither (prep_ctx->dither,
                       prep_ctx->dest_pixels,
                       prep_ctx->dest_width,
                       batch->first_row,
                       batch->n_rows);
    }
    else if (prep_ctx->dither->mode == CHAFA_DITHER_MODE_DIFFUSION)
    {
        fs_dither (prep_ctx->dither,
                   prep_ctx->palette,
                   prep_ctx->color_space,
                   prep_ctx->dest_pixels,
                   prep_ctx->dest_width,
                   batch->first_row,
                   batch->n_rows);
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
    gint n_batches;
    gint batch_unit = 1;

    /* Second pass
     * -----------
     *
     * - Normalization (optional)
     * - Dithering (optional)
     * - Color space conversion; DIN99d (optional)
     */

    if (!need_pass_2 (prep_ctx))
        return;

    n_batches = chafa_get_n_actual_threads ();

    /* Floyd-Steinberg diffusion needs the batch size to be a multiple of the
     * grain height. It also needs to run in a single thread to propagate the
     * quantization error correctly. */
    if (prep_ctx->dither->mode == CHAFA_DITHER_MODE_DIFFUSION)
    {
        n_batches = 1;
        batch_unit = 1 << prep_ctx->dither->grain_height_shift;
    }

    chafa_process_batches (prep_ctx,
                           (GFunc) prepare_pixels_2_worker,
                           NULL,  /* _post */
                           prep_ctx->dest_height,
                           n_batches,
                           batch_unit);
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
                                      gint dest_height,
                                      gint cell_width,
                                      gint cell_height,
                                      ChafaAlign halign,
                                      ChafaAlign valign,
                                      ChafaTuck tuck)
{
    PrepareContext prep_ctx = { 0 };
    gint placement_x, placement_y;
    gint placement_width, placement_height;

    /* Convert the destination dimensions from symbol matrix geometry to real
     * geometry (just for the calculation) for correct image sizing. */
    chafa_tuck_and_align (src_width, src_height,
                          (dest_width / CHAFA_SYMBOL_WIDTH_PIXELS) * cell_width,
                          (dest_height / CHAFA_SYMBOL_HEIGHT_PIXELS) * cell_height,
                          halign, valign,
                          tuck,
                          &placement_x, &placement_y,
                          &placement_width, &placement_height);

    /* Rounding the placement edges to cell boundaries prevents artifacts
     * in the first/last row/col containing the actual image,
     * when tuck == FIT or SHRINK_TO_FIT. */

    /* First image row/col rounds *down* to the nearest cell boundary. */
    placement_x -= placement_x % cell_width;
    placement_y -= placement_y % cell_height;

    /* Last image row/col rounds *up* to the nearest cell boundary.
     *
     * Note: If the left/top edge is on a cell boundary (which it already is),
     * and the width/height is a multiple of the cell width/height,
     * then the right/bottom edge is also on a cell boundary. */
    placement_width = round_up_to_multiple_of (placement_width, cell_width);
    placement_height = round_up_to_multiple_of (placement_height, cell_height);

    /* Convert the placement dimensions from real geometry to symbol matrix geometry. */
    placement_x = (placement_x / cell_width) * CHAFA_SYMBOL_WIDTH_PIXELS,
    placement_y = (placement_y / cell_height) * CHAFA_SYMBOL_HEIGHT_PIXELS,
    placement_width = (placement_width / cell_width) * CHAFA_SYMBOL_WIDTH_PIXELS,
    placement_height = (placement_height / cell_height) * CHAFA_SYMBOL_HEIGHT_PIXELS,

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

    prep_ctx.scale_ctx = smol_scale_new_full (/* Source */
                                              prep_ctx.src_pixels,
                                              (SmolPixelType) prep_ctx.src_pixel_type,
                                              prep_ctx.src_width,
                                              prep_ctx.src_height,
                                              prep_ctx.src_rowstride,
                                              /* Fill */
                                              NULL,
                                              SMOL_PIXEL_RGBA8_UNASSOCIATED,
                                              /* Destination */
                                              NULL,
                                              SMOL_PIXEL_RGBA8_UNASSOCIATED,  /* FIXME: Premul */
                                              prep_ctx.dest_width,
                                              prep_ctx.dest_height,
                                              prep_ctx.dest_width * sizeof (guint32),
                                              /* Placement */
                                              placement_x * SMOL_SUBPIXEL_MUL,
                                              placement_y * SMOL_SUBPIXEL_MUL,
                                              placement_width * SMOL_SUBPIXEL_MUL,
                                              placement_height * SMOL_SUBPIXEL_MUL,
                                              /* Extra args */
                                              SMOL_COMPOSITE_SRC_CLEAR_DEST,
                                              SMOL_NO_FLAGS,
                                              NULL,
                                              &prep_ctx);

    prepare_pixels_pass_1 (&prep_ctx);
    prepare_pixels_pass_2 (&prep_ctx);

    smol_scale_destroy (prep_ctx.scale_ctx);
}

void
chafa_sort_pixel_index_by_channel (guint8 *index, const ChafaPixel *pixels, gint n_pixels, gint ch)
{
    guint8 buckets [256] [64];
    guint8 bucket_size [256] = { 0 };
    gint i, j, k;

    g_assert (n_pixels <= 64);

    for (i = 0; i < n_pixels; i++)
    {
        guint8 bucket = pixels [i].col.ch [ch];
        buckets [bucket] [bucket_size [bucket]++] = i;
    }

    for (i = 0, k = 0; i < 256; i++)
    {
        for (j = 0; j < bucket_size [i]; j++)
            index [k++] = buckets [i] [j];
    }
}
