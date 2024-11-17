/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2019-2024 Hans Petter Jansson
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

#include <stdlib.h>  /* abs */
#include <string.h>  /* memcpy, memset */
#include <math.h>  /* pow, cbrt, log, sqrt, atan2, cos, sin */
#include "chafa.h"
#include "internal/chafa-private.h"

#define DEBUG(x)

/* ------------------------ *
 * Quality level parameters *
 * ------------------------ */

typedef struct
{
    gfloat min_quality;
    gint algorithm;  /* Currently unused */

    /* Number of samples to extract from the input. Samples are evenly
     * distributed across the image. This value is advisory -- we may extract
     * slightly more or fewer. If the samples are close together (small step
     * size), or too many samples are below the alpha threshold, we revert
     * to 1:1 population sampling. */
    gint n_samples;

    /* Number of high-order bits to grab from each color channel when
     * populating the initial bins. This determines the number of bins:
     *
     * n_bins = 2^(bits_per_ch * 3)
     *
     * In order to limit cache pollution, we use u16 to store bin indexes,
     * limiting us to 65536 bins. The upper limit for bits_per_ch is thus 5,
     * resulting in 32768 bins. We could use an extra bit for green (i.e.
     * RGB565), but it's probably not worth the effort, and it increases the
     * risk of oversaturating the L2 cache, at which point we become slower
     * than your average dog. We'd also have to respect the G_MAXUINT16
     * sentinel (effectively limiting us to 65535 bins). */
    gint bits_per_ch;
}
QualityParams;

static const QualityParams quality_params [] =
{
    {  .0f, 0, (1 << 14),  3 },  /* -w 1 */
    {  .1f, 0, (1 << 15),  3 },  /* -w 2 */
    {  .2f, 0, (1 << 16),  4 },  /* -w 3 */
    {  .3f, 0, (1 << 17),  4 },  /* -w 4 */
    { .45f, 0, (1 << 18),  4 },  /* -w 5 */
    {  .6f, 0, (1 << 19),  5 },  /* -w 6 */
    {  .7f, 0, (1 << 20),  5 },  /* -w 7 */
    {  .8f, 0, (1 << 21),  5 },  /* -w 8 */
    { .95f, 0, (1 << 26),  5 },  /* -w 9 */

    { -.1f, -1, -1, -1 }
};

static const QualityParams *
get_quality_params (gfloat quality)
{
    const QualityParams *params = &quality_params [0];
    gint i;

    for (i = 0; quality_params [i].algorithm >= 0
                && quality >= quality_params [i].min_quality; i++)
    {
        params = &quality_params [i];
    }

    return params;
}

/* ---------------- *
 * Color candidates *
 * ---------------- */

/* Some situations (like fill symbols) call for both a best and a second-best
 * match. ChafaColorCandidates is used to track and return these. */

static void
init_candidates (ChafaColorCandidates *candidates)
{
    candidates->index [0] = candidates->index [1] = -1;
    candidates->error [0] = candidates->error [1] = G_MAXINT;
}

static gboolean
update_candidates (ChafaColorCandidates *candidates, gint index, gint error)
{
    if (error < candidates->error [0])
    {
        candidates->index [1] = candidates->index [0];
        candidates->index [0] = index;
        candidates->error [1] = candidates->error [0];
        candidates->error [0] = error;
        return TRUE;
    }
    else if (error < candidates->error [1])
    {
        candidates->index [1] = index;
        candidates->error [1] = error;
        return TRUE;
    }

    return FALSE;
}

/* -------------------- *
 * Fixed system palette *
 * -------------------- */

static const guint32 term_colors_256 [CHAFA_PALETTE_INDEX_MAX] =
{
    /* First 16 colors; these are usually set by the terminal and can vary quite a
     * bit. We try to strike a balance. */

    0x000000, 0x800000, 0x007000, 0x707000, 0x000070, 0x700070, 0x007070, 0xc0c0c0,
    0x404040, 0xff0000, 0x00ff00, 0xffff00, 0x0000ff, 0xff00ff, 0x00ffff, 0xffffff,

    /* 240 universal colors; a 216-entry color cube followed by 24 grays */

    0x000000, 0x00005f, 0x000087, 0x0000af, 0x0000d7, 0x0000ff, 0x005f00, 0x005f5f,
    0x005f87, 0x005faf, 0x005fd7, 0x005fff, 0x008700, 0x00875f, 0x008787, 0x0087af,

    0x0087d7, 0x0087ff, 0x00af00, 0x00af5f, 0x00af87, 0x00afaf, 0x00afd7, 0x00afff,
    0x00d700, 0x00d75f, 0x00d787, 0x00d7af, 0x00d7d7, 0x00d7ff, 0x00ff00, 0x00ff5f,

    0x00ff87, 0x00ffaf, 0x00ffd7, 0x00ffff, 0x5f0000, 0x5f005f, 0x5f0087, 0x5f00af,
    0x5f00d7, 0x5f00ff, 0x5f5f00, 0x5f5f5f, 0x5f5f87, 0x5f5faf, 0x5f5fd7, 0x5f5fff,

    0x5f8700, 0x5f875f, 0x5f8787, 0x5f87af, 0x5f87d7, 0x5f87ff, 0x5faf00, 0x5faf5f,
    0x5faf87, 0x5fafaf, 0x5fafd7, 0x5fafff, 0x5fd700, 0x5fd75f, 0x5fd787, 0x5fd7af,

    0x5fd7d7, 0x5fd7ff, 0x5fff00, 0x5fff5f, 0x5fff87, 0x5fffaf, 0x5fffd7, 0x5fffff,
    0x870000, 0x87005f, 0x870087, 0x8700af, 0x8700d7, 0x8700ff, 0x875f00, 0x875f5f,

    0x875f87, 0x875faf, 0x875fd7, 0x875fff, 0x878700, 0x87875f, 0x878787, 0x8787af,
    0x8787d7, 0x8787ff, 0x87af00, 0x87af5f, 0x87af87, 0x87afaf, 0x87afd7, 0x87afff,

    0x87d700, 0x87d75f, 0x87d787, 0x87d7af, 0x87d7d7, 0x87d7ff, 0x87ff00, 0x87ff5f,
    0x87ff87, 0x87ffaf, 0x87ffd7, 0x87ffff, 0xaf0000, 0xaf005f, 0xaf0087, 0xaf00af,

    0xaf00d7, 0xaf00ff, 0xaf5f00, 0xaf5f5f, 0xaf5f87, 0xaf5faf, 0xaf5fd7, 0xaf5fff,
    0xaf8700, 0xaf875f, 0xaf8787, 0xaf87af, 0xaf87d7, 0xaf87ff, 0xafaf00, 0xafaf5f,

    0xafaf87, 0xafafaf, 0xafafd7, 0xafafff, 0xafd700, 0xafd75f, 0xafd787, 0xafd7af,
    0xafd7d7, 0xafd7ff, 0xafff00, 0xafff5f, 0xafff87, 0xafffaf, 0xafffd7, 0xafffff,

    0xd70000, 0xd7005f, 0xd70087, 0xd700af, 0xd700d7, 0xd700ff, 0xd75f00, 0xd75f5f,
    0xd75f87, 0xd75faf, 0xd75fd7, 0xd75fff, 0xd78700, 0xd7875f, 0xd78787, 0xd787af,

    0xd787d7, 0xd787ff, 0xd7af00, 0xd7af5f, 0xd7af87, 0xd7afaf, 0xd7afd7, 0xd7afff,
    0xd7d700, 0xd7d75f, 0xd7d787, 0xd7d7af, 0xd7d7d7, 0xd7d7ff, 0xd7ff00, 0xd7ff5f,

    0xd7ff87, 0xd7ffaf, 0xd7ffd7, 0xd7ffff, 0xff0000, 0xff005f, 0xff0087, 0xff00af,
    0xff00d7, 0xff00ff, 0xff5f00, 0xff5f5f, 0xff5f87, 0xff5faf, 0xff5fd7, 0xff5fff,

    0xff8700, 0xff875f, 0xff8787, 0xff87af, 0xff87d7, 0xff87ff, 0xffaf00, 0xffaf5f,
    0xffaf87, 0xffafaf, 0xffafd7, 0xffafff, 0xffd700, 0xffd75f, 0xffd787, 0xffd7af,

    0xffd7d7, 0xffd7ff, 0xffff00, 0xffff5f, 0xffff87, 0xffffaf, 0xffffd7, 0xffffff,
    0x080808, 0x121212, 0x1c1c1c, 0x262626, 0x303030, 0x3a3a3a, 0x444444, 0x4e4e4e,

    0x585858, 0x626262, 0x6c6c6c, 0x767676, 0x808080, 0x8a8a8a, 0x949494, 0x9e9e9e,
    0xa8a8a8, 0xb2b2b2, 0xbcbcbc, 0xc6c6c6, 0xd0d0d0, 0xdadada, 0xe4e4e4, 0xeeeeee,

    /* Special colors */

    0x808080,  /* Transparent */
    0xffffff,  /* Terminal's default foreground */
    0x000000,  /* Terminal's default background */
};

static ChafaPaletteColor fixed_palette_256 [CHAFA_PALETTE_INDEX_MAX];
static guchar color_cube_216_channel_index [256];
static gboolean palette_initialized;

static const ChafaColor *
get_fixed_palette_color (guint index, ChafaColorSpace color_space)
{
    return &fixed_palette_256 [index].col [color_space];
}

void
chafa_init_palette (void)
{
    gint i;

    if (palette_initialized)
        return;

    for (i = 0; i < CHAFA_PALETTE_INDEX_MAX; i++)
    {
        chafa_unpack_color (term_colors_256 [i], &fixed_palette_256 [i].col [0]);
        chafa_color_rgb_to_din99d (&fixed_palette_256 [i].col [0], &fixed_palette_256 [i].col [1]);

        fixed_palette_256 [i].col [0].ch [3] = 0xff;  /* Fully opaque */
        fixed_palette_256 [i].col [1].ch [3] = 0xff;  /* Fully opaque */
    }

    /* Transparent color */
    fixed_palette_256 [CHAFA_PALETTE_INDEX_TRANSPARENT].col [0].ch [3] = 0x00;
    fixed_palette_256 [CHAFA_PALETTE_INDEX_TRANSPARENT].col [1].ch [3] = 0x00;

    for (i = 0; i < 0x5f / 2; i++)
        color_cube_216_channel_index [i] = 0;
    for ( ; i < (0x5f + 0x87) / 2; i++)
        color_cube_216_channel_index [i] = 1;
    for ( ; i < (0x87 + 0xaf) / 2; i++)
        color_cube_216_channel_index [i] = 2;
    for ( ; i < (0xaf + 0xd7) / 2; i++)
        color_cube_216_channel_index [i] = 3;
    for ( ; i < (0xd7 + 0xff) / 2; i++)
        color_cube_216_channel_index [i] = 4;
    for ( ; i <= 0xff; i++)
        color_cube_216_channel_index [i] = 5;

    palette_initialized = TRUE;
}

static gint
update_candidates_with_color_index_diff (ChafaColorCandidates *candidates, ChafaColorSpace color_space, const ChafaColor *color, gint index)
{
    const ChafaColor *palette_color;
    gint error;

    palette_color = get_fixed_palette_color (index, color_space);
    error = chafa_color_diff_fast (color, palette_color);
    update_candidates (candidates, index, error);

    return error;
}

static void
pick_color_fixed_216_cube (const ChafaColor *color, ChafaColorSpace color_space, ChafaColorCandidates *candidates)
{
    gint i;

    g_assert (color_space == CHAFA_COLOR_SPACE_RGB);

    i = 16 + (color_cube_216_channel_index [color->ch [0]] * 6 * 6
              + color_cube_216_channel_index [color->ch [1]] * 6
              + color_cube_216_channel_index [color->ch [2]]);

    update_candidates_with_color_index_diff (candidates, color_space, color, i);
}

static void
pick_color_fixed_24_grays (const ChafaColor *color, ChafaColorSpace color_space, ChafaColorCandidates *candidates)
{
    const ChafaColor *palette_color;
    gint error, last_error = G_MAXINT;
    gint step, i;

    g_assert (color_space == CHAFA_COLOR_SPACE_RGB);

    i = 232 + 12;
    last_error = update_candidates_with_color_index_diff (candidates, color_space, color, i);

    palette_color = get_fixed_palette_color (i + 1, color_space);
    error = chafa_color_diff_fast (color, palette_color);
    if (error < last_error)
    {
        update_candidates (candidates, i, error);
        last_error = error;
        step = 1;
        i++;
    }
    else
    {
        step = -1;
    }

    do
    {
        i += step;
        palette_color = get_fixed_palette_color (i, color_space);

        error = chafa_color_diff_fast (color, palette_color);
        if (error > last_error)
            break;

        update_candidates (candidates, i, error);
        last_error = error;
    }
    while (i >= 232 && i <= 255);
}

static void
pick_color_fixed_16 (const ChafaColor *color, ChafaColorSpace color_space, ChafaColorCandidates *candidates)
{
    gint i;

    for (i = 0; i < 16; i++)
    {
        update_candidates_with_color_index_diff (candidates, color_space, color, i);
    }
}

static void
pick_color_fixed_8 (const ChafaColor *color, ChafaColorSpace color_space, ChafaColorCandidates *candidates)
{
    gint i;

    for (i = 0; i < 8; i++)
    {
        update_candidates_with_color_index_diff (candidates, color_space, color, i);
    }
}

static void
pick_color_fixed_256 (const ChafaColor *color, ChafaColorSpace color_space, ChafaColorCandidates *candidates)
{
    gint i;

    if (color_space == CHAFA_COLOR_SPACE_RGB)
    {
        pick_color_fixed_216_cube (color, color_space, candidates);
        pick_color_fixed_24_grays (color, color_space, candidates);

        /* Do this last so ties are broken in favor of high-index colors. */
        pick_color_fixed_16 (color, color_space, candidates);
    }
    else
    {
        for (i = 0; i < 256; i++)
        {
            update_candidates_with_color_index_diff (candidates, color_space, color, i);
        }
    }
}

static void
pick_color_fixed_240 (const ChafaColor *color, ChafaColorSpace color_space, ChafaColorCandidates *candidates)
{
    gint i;

    if (color_space == CHAFA_COLOR_SPACE_RGB)
    {
        pick_color_fixed_216_cube (color, color_space, candidates);
        pick_color_fixed_24_grays (color, color_space, candidates);
    }
    else
    {
        /* Check color cube, but not lower 16, bg or fg. Slow! */
        for (i = 16; i < 256; i++)
        {
            update_candidates_with_color_index_diff (candidates, color_space, color, i);
        }
    }
}

/* Pick the best approximation of color from a palette consisting of
 * fg_color and bg_color */
static void
pick_color_fixed_fgbg (const ChafaColor *color,
                 const ChafaColor *fg_color, const ChafaColor *bg_color,
                 ChafaColorCandidates *candidates)
{
    gint error;

    error = chafa_color_diff_fast (color, fg_color);
    update_candidates (candidates, CHAFA_PALETTE_INDEX_FG, error);

    error = chafa_color_diff_fast (color, bg_color);
    update_candidates (candidates, CHAFA_PALETTE_INDEX_BG, error);
}

/* ----------------------------------- *
 * Pairwise nearest neighbor quantizer *
 * ----------------------------------- */

/* Implementation inspired by nQuant by Mark Tyler, Dmitry Groshev and
 * Miller Cy Chan.
 *
 * There's a nice description of the PNN algorithm and several fast
 * variants in DOI:10.1117/1.1412423 and DOI:10.1117/1.1604396. */

#define RED_WEIGHT_32f .299f
#define GREEN_WEIGHT_32f .587f
#define BLUE_WEIGHT_32f .114f
#define RATIO .5f

static const ChafaVec3f32 pnn_coeffs [3] =
{
    CHAFA_VEC3F32_INIT (RED_WEIGHT_32f, GREEN_WEIGHT_32f, BLUE_WEIGHT_32f),
    CHAFA_VEC3F32_INIT (-0.14713f, -0.28886f,  0.436f),
    CHAFA_VEC3F32_INIT (0.615f, -0.51499f, -0.10001f)
};

typedef guint16 PnnBinIndex;

typedef struct
{
    ChafaVec3f32 accum;
    gfloat err;
    gfloat count;
    PnnBinIndex nearest, next, prev, tm, mtm;
}
PnnBin;

/* 3 <= bits_per_ch <= 8. However, we're limited to <= 5 bits elsewhere. */
static gint
color_to_index (const ChafaColor *color, gint bits_per_ch)
{
    guint8 mask = 0xff << (8 - bits_per_ch);

    return ((color->ch [0] & mask) << (bits_per_ch * 2 - (8 - bits_per_ch)))
        | ((color->ch [1] & mask) << (bits_per_ch - (8 - bits_per_ch)))
        | ((color->ch [2] & mask) >> (8 - bits_per_ch));
}

static gfloat
quanfn (gfloat count, gint quan_rt)
{
    if (quan_rt > 0)
    {
        if (quan_rt > 1)
            return (gfloat) (gint) sqrtf (count);
        return sqrtf (count);
    }
    if (quan_rt < 0)
        return (gfloat) (gint) cbrtf (count);

    return count;
}

static void
find_nearest (PnnBin *bins, PnnBinIndex index, const ChafaVec3f32 *rgb_weights)
{
    PnnBin *bin1 = &bins [index];
    gfloat err = G_MAXFLOAT;
    PnnBinIndex nearest = 0;
    PnnBinIndex i, j;

    for (i = bin1->next; i; i = bins [i].next)
    {
        gfloat bin2_count = bins [i].count;
        gfloat nerr2 = (bin1->count * bin2_count) / (bin1->count + bin2_count);
        gfloat nerr = .0f;
        ChafaVec3f32 tv;

        if (nerr2 >= err)
            continue;

        chafa_vec3f32_sub (&tv, &bins [i].accum, &bin1->accum);
        chafa_vec3f32_hadamard (&tv, &tv, &tv);
        chafa_vec3f32_hadamard (&tv, &tv, rgb_weights);
        chafa_vec3f32_mul_scalar (&tv, &tv, nerr2 * (1 - RATIO));
        nerr += chafa_vec3f32_sum_to_scalar (&tv);
        if (nerr >= err)
            continue;

        for (j = 0; j < 3; j++)
        {
            chafa_vec3f32_sub (&tv, &bins [i].accum, &bin1->accum);
            chafa_vec3f32_hadamard (&tv, &tv, &pnn_coeffs [j]);
            chafa_vec3f32_hadamard (&tv, &tv, &tv);
            chafa_vec3f32_mul_scalar (&tv, &tv, nerr2 * RATIO);
            nerr += chafa_vec3f32_sum_to_scalar (&tv);
            if (nerr >= err)
                break;
        }

        if (nerr < err)
        {
            err = nerr;
            nearest = i;
        }
    }

    bin1->err = err;
    bin1->nearest = nearest;
}

static void
vec3f32_add_color (ChafaVec3f32 *out, const ChafaColor *col)
{
    out->v [0] += col->ch [0];
    out->v [1] += col->ch [1];
    out->v [2] += col->ch [2];
    /* Ignore alpha */
}

static void
color_from_vec3f32_trunc (ChafaColor *col, const ChafaVec3f32 *v)
{
    col->ch [0] = v->v [0];
    col->ch [1] = v->v [1];
    col->ch [2] = v->v [2];
    /* Ignore alpha */
}

static gint
sample_to_bins (PnnBin *bins, gconstpointer pixels, size_t n_pixels, gint step,
                gint bits_per_ch, gint alpha_threshold)
{
    const ChafaColor *p = (const ChafaColor *) pixels;
    gint n_samples = 0;
    size_t i;

    for (i = 0; i < n_pixels; i += step)
    {
        const ChafaColor col = p [i];
        PnnBin *tb;

        if (col.ch [3] < alpha_threshold)
            continue;

        tb = &bins [color_to_index (&col, bits_per_ch)];
        vec3f32_add_color (&tb->accum, &col);
        tb->count += 1.0f;
        n_samples++;
    }

    return n_samples;
}

static gint
pnn_palette (ChafaPalette *pal, gconstpointer pixels,
             gint n_pixels, gint n_cols,
             gint bits_per_ch, gint sample_step,
             gint alpha_threshold)
{
    ChafaVec3f32 rgb_weights =
        CHAFA_VEC3F32_INIT (RED_WEIGHT_32f, GREEN_WEIGHT_32f, BLUE_WEIGHT_32f);
    PnnBin *bins;
    PnnBinIndex *heap;
    gfloat weight = 1.0f;
    gfloat err;
    gint quan_rt = 1;
    gint max_bins, n_bins;
    gint extbins;
    gint i, j, k;

    g_assert (bits_per_ch >= 3);
    g_assert (bits_per_ch <= 5);

    max_bins = 1 << (bits_per_ch * 3);
    bins = g_new0 (PnnBin, max_bins);

    /* --- Extract samples and assign to bins --- */

    if (sample_to_bins (bins, pixels, n_pixels, sample_step, bits_per_ch,
                        alpha_threshold) < 256)
    {
        if (sample_step == 1)
            return 0;

        /* Too many transparent pixels. Try again at maximum density */
        memset (bins, 0, max_bins * sizeof (PnnBin));
        if (sample_to_bins (bins, pixels, n_pixels, 1, bits_per_ch,
                            alpha_threshold) <= 0)
            return 0;
    }

    /* --- Count active bins and average their colors --- */

    for (i = 0, n_bins = 0; i < max_bins; i++)
    {
        PnnBin *tb = &bins [i];

        if (bins [i].count <= .0f)
            continue;

        chafa_vec3f32_mul_scalar (&tb->accum, &tb->accum, 1.0f / tb->count);
        bins [n_bins++] = *tb;
    }

    /* --- Set up weights and bin counts --- */

    if (n_cols < 16)
        quan_rt = -1;

    weight = MIN (0.9f, n_cols * 1.0f / n_bins);
    if (weight < .03f && rgb_weights.v [1] < 1.0f
        && rgb_weights.v [1] >= pnn_coeffs [0].v [1])
    {
        chafa_vec3f32_set (&rgb_weights, 1.0f, 1.0f, 1.0f);
        if (n_cols >= 64)
            quan_rt = 0;
    }

    if (quan_rt > 0 && n_cols < 64)
        quan_rt = 2;

    for (j = 0; j < n_bins - 1; j++)
    {
        bins [j].next = j + 1;
        bins [j + 1].prev = j;
        bins [j].count = quanfn (bins [j].count, quan_rt);
    }
    bins [j].count = quanfn (bins [j].count, quan_rt);

    /* --- Set up heap --- */

    heap = g_new0 (PnnBinIndex, max_bins + 1);

    for (i = 0; i < n_bins; i++)
    {
        PnnBinIndex h, l, l2;
        gfloat err;

        find_nearest (bins, i, &rgb_weights);
        err = bins [i].err;

        heap [0]++;
        for (l = heap [0]; l > 1; l = l2)
        {
            l2 = l >> 1;
            h = heap [l2];
            if (bins [h].err <= err)
                break;
            heap [l] = h;
        }

        heap [l] = i;
    }

    /* --- Merge bins iteratively --- */

    extbins = n_bins - n_cols;

    for (i = 0; i < extbins; )
    {
        ChafaVec3f32 tv1;
        PnnBin *tb;
        PnnBin *nb;
        gfloat n1;
        gfloat n2;
        gfloat d;
        PnnBinIndex b1;

        for (;;)
        {
            PnnBinIndex h, l, l2;

            b1 = heap [1];
            tb = &bins [b1];

            if ((tb->tm >= tb->mtm) && (bins [tb->nearest].mtm <= tb->tm))
                break;

            if (tb->mtm == G_MAXUINT16)
            {
                b1 = heap [1] = heap [heap [0]];
                heap [0]--;
            }
            else
            {
                find_nearest (bins, b1, &rgb_weights);
                tb->tm = i;
            }

            err = bins [b1].err;

            for (l = 1, l2 = 2; l2 <= heap [0]; l = l2, l2 = l * 2)
            {
                if ((l2 < heap [0]) && (bins [heap [l2]].err > bins [heap [l2 + 1]].err))
                    l2++;
                h = heap [l2];
                if (err <= bins [h].err)
                    break;
                heap [l] = h;
            }
            heap [l] = b1;
        }

        tb = &bins [b1];
        nb = &bins [tb->nearest];
        n1 = tb->count;
        n2 = nb->count;
        d = 1.0f / (n1 + n2);

        chafa_vec3f32_mul_scalar (&tb->accum, &tb->accum, n1);
        chafa_vec3f32_mul_scalar (&tv1, &nb->accum, n2);
        chafa_vec3f32_add (&tb->accum, &tb->accum, &tv1);
        chafa_vec3f32_round (&tb->accum, &tb->accum);
        chafa_vec3f32_mul_scalar (&tb->accum, &tb->accum, d);

        tb->count += n2;
        tb->mtm = ++i;

        bins [nb->prev].next = nb->next;
        bins [nb->next].prev = nb->prev;
        nb->mtm = G_MAXUINT16;
    }

    /* --- Export final colors --- */

    for (i = 0, k = 0; ; k++)
    {
        ChafaColor col = { 0 };

        color_from_vec3f32_trunc (&col, &bins [i].accum);
        col.ch [3] = 0xff;

        pal->colors [k].col [CHAFA_COLOR_SPACE_RGB] = col;

        i = bins [i].next;
        if (!i)
            break;
    }

    g_free (heap);
    g_free (bins);

    /* We may produce fewer colors than requested */
    return k + 1;
}

static void
gen_din99d_color_space (ChafaPalette *palette)
{
    gint i;

    for (i = 0; i < palette->n_colors; i++)
    {
        chafa_color_rgb_to_din99d (&palette->colors [i].col [CHAFA_COLOR_SPACE_RGB],
                                   &palette->colors [i].col [CHAFA_COLOR_SPACE_DIN99D]);
    }
}

static void
gen_table (ChafaPalette *palette, ChafaColorSpace color_space)
{
    gint i;

    for (i = 0; i < palette->n_colors; i++)
    {
        const ChafaColor *col;

        if (i == palette->transparent_index)
            continue;

        col = &palette->colors [i].col [color_space];

        chafa_color_table_set_pen_color (&palette->table [color_space], i,
                                         col->ch [0] | (col->ch [1] << 8) | (col->ch [2] << 16));
    }

    chafa_color_table_sort (&palette->table [color_space]);
}

static void
clean_up (ChafaPalette *palette_out)
{
    gint i, j;
    gint best_diff = G_MAXINT;
    gint best_pair = 1;

    /* Reserve 0th pen for transparency and move colors up.
     * Eliminate duplicates and colors that would be the same in
     * sixel representation (0..100). */

    DEBUG (g_printerr ("Colors before: %d\n", palette_out->n_colors));

    for (i = 1, j = 1; i < palette_out->n_colors; i++)
    {
        ChafaColor *a, *b;
        gint diff, t;

        a = &palette_out->colors [j - 1].col [CHAFA_COLOR_SPACE_RGB];
        b = &palette_out->colors [i].col [CHAFA_COLOR_SPACE_RGB];

        /* Dividing by 256 is strictly not correct, but it's close enough for
         * comparison purposes, and a lot faster too. */
        t = (gint) (a->ch [0] * 100) / 256 - (gint) (b->ch [0] * 100) / 256;
        diff = t * t;
        t = (gint) (a->ch [1] * 100) / 256 - (gint) (b->ch [1] * 100) / 256;
        diff += t * t;
        t = (gint) (a->ch [2] * 100) / 256 - (gint) (b->ch [2] * 100) / 256;
        diff += t * t;

        if (diff == 0)
        {
            DEBUG (g_printerr ("%d and %d are the same\n", j - 1, i));
            continue;
        }
        else if (diff < best_diff)
        {
            best_pair = j - 1;
            best_diff = diff;
        }

        palette_out->colors [j++] = palette_out->colors [i];
    }

    palette_out->n_colors = j;

    DEBUG (g_printerr ("Colors after: %d\n", palette_out->n_colors));

    g_assert (palette_out->n_colors >= 0 && palette_out->n_colors <= 256);

    if (palette_out->transparent_index < 256)
    {
        if (palette_out->n_colors < 256)
        {
            DEBUG (g_printerr ("Color 0 moved to end (%d)\n", palette_out->n_colors));
            palette_out->colors [palette_out->n_colors] = palette_out->colors [palette_out->transparent_index];
            palette_out->n_colors++;
        }
        else
        {
            /* Delete one color to make room for transparency */
            palette_out->colors [best_pair] = palette_out->colors [palette_out->transparent_index];
            DEBUG (g_printerr ("Color 0 replaced %d\n", best_pair));
        }
    }
}

/* --- *
 * API *
 * --- */

void
chafa_palette_init (ChafaPalette *palette_out, ChafaPaletteType type)
{
    gint i;

    chafa_init_palette ();
    palette_out->type = type;
    palette_out->transparent_index = CHAFA_PALETTE_INDEX_TRANSPARENT;

    for (i = 0; i < CHAFA_PALETTE_INDEX_MAX; i++)
    {
        palette_out->colors [i].col [CHAFA_COLOR_SPACE_RGB] = *get_fixed_palette_color (i, CHAFA_COLOR_SPACE_RGB);
        palette_out->colors [i].col [CHAFA_COLOR_SPACE_DIN99D] = *get_fixed_palette_color (i, CHAFA_COLOR_SPACE_DIN99D);
    }

    switch (type)
    {
        case CHAFA_PALETTE_TYPE_FIXED_FGBG:
            palette_out->first_color = CHAFA_PALETTE_INDEX_FG;
            palette_out->n_colors = 2;
            break;

        case CHAFA_PALETTE_TYPE_FIXED_8:
            palette_out->n_colors = 8;
            break;

        case CHAFA_PALETTE_TYPE_FIXED_16:
            palette_out->n_colors = 16;
            break;

        case CHAFA_PALETTE_TYPE_FIXED_240:
            palette_out->first_color = 16;
            palette_out->n_colors = 240;
            break;

        case CHAFA_PALETTE_TYPE_FIXED_256:
            palette_out->first_color = 0;
            palette_out->n_colors = 256;
            break;

        case CHAFA_PALETTE_TYPE_DYNAMIC_256:
            for (i = 0; i < CHAFA_COLOR_SPACE_MAX; i++)
                chafa_color_table_init (&palette_out->table [i]);
            break;
    }
}

void
chafa_palette_deinit (ChafaPalette *palette)
{
    gint i;

    if (palette->type == CHAFA_PALETTE_TYPE_DYNAMIC_256)
    {
        for (i = 0; i < CHAFA_COLOR_SPACE_MAX; i++)
            chafa_color_table_deinit (&palette->table [i]);
    }
}

gint
chafa_palette_get_first_color (const ChafaPalette *palette)
{
    return palette->first_color;
}

gint
chafa_palette_get_n_colors (const ChafaPalette *palette)
{
    return palette->n_colors;
}

void
chafa_palette_copy (const ChafaPalette *src, ChafaPalette *dest)
{
    memcpy (dest, src, sizeof (*dest));
}

/* pixels must point to RGBA8888 data to sample */
/* FIXME: Rowstride etc? */
void
chafa_palette_generate (ChafaPalette *palette_out, gconstpointer pixels, gint n_pixels,
                        ChafaColorSpace color_space, gfloat quality)
{
    const QualityParams *params;
    gint step;

    if (palette_out->type != CHAFA_PALETTE_TYPE_DYNAMIC_256)
        return;

    /* --- Determine quality parameters --- */

    if (quality < 0.0f)
        quality = 0.5f;
    quality = CLAMP (quality, 0.0f, 1.0f);

    params = get_quality_params (quality);
    step = n_pixels / params->n_samples;

    /* If step is small, revert to dense sampling. We're going to fetch
     * every cache line anyway, might as well make the most of it. */
    if (step <= 4)
        step = 1;

    /* --- Generate --- */

    palette_out->n_colors = pnn_palette (palette_out,
                                         pixels,
                                         n_pixels,
                                         255,
                                         params->bits_per_ch,
                                         step,
                                         palette_out->alpha_threshold);
    clean_up (palette_out);
    gen_table (palette_out, CHAFA_COLOR_SPACE_RGB);

    if (color_space == CHAFA_COLOR_SPACE_DIN99D)
    {
        gen_din99d_color_space (palette_out);
        gen_table (palette_out, CHAFA_COLOR_SPACE_DIN99D);
    }
}

gint
chafa_palette_lookup_nearest (const ChafaPalette *palette, ChafaColorSpace color_space,
                              const ChafaColor *color, ChafaColorCandidates *candidates)
{
    if (palette->type == CHAFA_PALETTE_TYPE_DYNAMIC_256)
    {
        gint result;

        /* Transparency */
        if (color->ch [3] < palette->alpha_threshold)
            return palette->transparent_index;

        result = chafa_color_table_find_nearest_pen (&palette->table [color_space],
                                                     color->ch [0]
                                                     | (color->ch [1] << 8)
                                                     | (color->ch [2] << 16));

        if (candidates)
        {
            /* The only consumer of multiple candidates is the cell canvas, and that
             * supports fixed palettes only. Therefore, in practice we'll never end up here.
             * Let's not leave a loose end, though... */

            candidates->index [0] = result;
            candidates->index [1] = result;
            candidates->error [0] = 0;
            candidates->error [1] = 0;
        }

        return result;
    }
    else
    {
        ChafaColorCandidates candidates_temp;

        if (!candidates)
            candidates = &candidates_temp;

        init_candidates (candidates);

        if (color->ch [3] < palette->alpha_threshold)
        {
            /* Transparency */
            candidates->index [0] = palette->transparent_index;
            candidates->index [1] = palette->transparent_index;
            candidates->error [0] = 0;
            candidates->error [1] = 0;
        }
        else if (palette->type == CHAFA_PALETTE_TYPE_FIXED_256)
            pick_color_fixed_256 (color, color_space, candidates);
        else if (palette->type == CHAFA_PALETTE_TYPE_FIXED_240)
            pick_color_fixed_240 (color, color_space, candidates);
        else if (palette->type == CHAFA_PALETTE_TYPE_FIXED_16)
            pick_color_fixed_16 (color, color_space, candidates);
        else if (palette->type == CHAFA_PALETTE_TYPE_FIXED_8)
            pick_color_fixed_8 (color, color_space, candidates);
        else /* CHAFA_PALETTE_TYPE_FIXED_FGBG */
            pick_color_fixed_fgbg (color,
                                   &palette->colors [CHAFA_PALETTE_INDEX_FG].col [color_space],
                                   &palette->colors [CHAFA_PALETTE_INDEX_BG].col [color_space],
                                   candidates);

        if (palette->transparent_index < 256)
        {
            if (candidates->index [0] == palette->transparent_index)
            {
                candidates->index [0] = candidates->index [1];
                candidates->error [0] = candidates->error [1];
            }
            else
            {
                if (candidates->index [0] == CHAFA_PALETTE_INDEX_TRANSPARENT)
                    candidates->index [0] = palette->transparent_index;
                if (candidates->index [1] == CHAFA_PALETTE_INDEX_TRANSPARENT)
                    candidates->index [1] = palette->transparent_index;
            }
        }

        return candidates->index [0];
    }

    g_assert_not_reached ();
}

gint
chafa_palette_lookup_with_error (const ChafaPalette *palette, ChafaColorSpace color_space,
                                 ChafaColor color, ChafaColorAccum *error_inout)
{
    ChafaColorAccum compensated_color;
    gint index;

    if (error_inout)
    {
        compensated_color.ch [0] = ((gint16) color.ch [0]) + ((error_inout->ch [0] * 0.9f) / 16);
        compensated_color.ch [1] = ((gint16) color.ch [1]) + ((error_inout->ch [1] * 0.9f) / 16);
        compensated_color.ch [2] = ((gint16) color.ch [2]) + ((error_inout->ch [2] * 0.9f) / 16);

        color.ch [0] = CLAMP (compensated_color.ch [0], 0, 255);
        color.ch [1] = CLAMP (compensated_color.ch [1], 0, 255);
        color.ch [2] = CLAMP (compensated_color.ch [2], 0, 255);
    }

    index = chafa_palette_lookup_nearest (palette, color_space, &color, NULL);

    if (error_inout)
    {
        if (index == palette->transparent_index)
        {
            memset (error_inout, 0, sizeof (*error_inout));
        }
        else
        {
            ChafaColor found_color = palette->colors [index].col [color_space];

            error_inout->ch [0] = ((gint16) compensated_color.ch [0]) - ((gint16) found_color.ch [0]);
            error_inout->ch [1] = ((gint16) compensated_color.ch [1]) - ((gint16) found_color.ch [1]);
            error_inout->ch [2] = ((gint16) compensated_color.ch [2]) - ((gint16) found_color.ch [2]);
        }
    }

    return index;
}

ChafaPaletteType
chafa_palette_get_type (const ChafaPalette *palette)
{
    return palette->type;
}

const ChafaColor *
chafa_palette_get_color (const ChafaPalette *palette, ChafaColorSpace color_space,
                         gint index)
{
    return &palette->colors [index].col [color_space];
}

void
chafa_palette_set_color (ChafaPalette *palette, gint index, const ChafaColor *color)
{
    palette->colors [index].col [CHAFA_COLOR_SPACE_RGB] = *color;
    chafa_color_rgb_to_din99d (&palette->colors [index].col [CHAFA_COLOR_SPACE_RGB],
                               &palette->colors [index].col [CHAFA_COLOR_SPACE_DIN99D]);
}

gint
chafa_palette_get_alpha_threshold (const ChafaPalette *palette)
{
    return palette->alpha_threshold;
}

void
chafa_palette_set_alpha_threshold (ChafaPalette *palette, gint alpha_threshold)
{
    palette->alpha_threshold = alpha_threshold;
}

gint
chafa_palette_get_transparent_index (const ChafaPalette *palette)
{
    return palette->transparent_index;
}

void
chafa_palette_set_transparent_index (ChafaPalette *palette, gint index)
{
    palette->transparent_index = index;
}

