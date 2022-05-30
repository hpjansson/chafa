/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2019-2022 Hans Petter Jansson
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

#include <stdlib.h>  /* abs */
#include <string.h>  /* memcpy, memset */
#include <math.h>  /* pow, cbrt, log, sqrt, atan2, cos, sin */
#include "chafa.h"
#include "internal/chafa-private.h"

#define DEBUG(x)

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

/* --------------------------------- *
 * Quantization for dynamic palettes *
 * --------------------------------- */

static gint
find_dominant_channel (gconstpointer pixels, gint n_pixels)
{
    const guint8 *p = pixels;
    guint8 min [4] = { G_MAXUINT8, G_MAXUINT8, G_MAXUINT8, G_MAXUINT8 };
    guint8 max [4] = { 0, 0, 0, 0 };
    guint16 diff [4];
    gint best;
    gint i;

    for (i = 0; i < n_pixels; i++)
    {
        /* This should yield branch-free code where possible */
        min [0] = MIN (min [0], *p);
        max [0] = MAX (max [0], *p);
        p++;
        min [1] = MIN (min [1], *p);
        max [1] = MAX (max [1], *p);
        p++;
        min [2] = MIN (min [2], *p);
        max [2] = MAX (max [2], *p);

        /* Skip alpha */
        p += 2;
    }

#if 1
    /* Multipliers for luminance */
    diff [0] = (max [0] - min [0]) * 30;
    diff [1] = (max [1] - min [1]) * 59;
    diff [2] = (max [2] - min [2]) * 11;
#else
    diff [0] = (max [0] - min [0]);
    diff [1] = (max [1] - min [1]);
    diff [2] = (max [2] - min [2]);
#endif

    /* If there are ties, prioritize thusly: G, R, B */

    best = 1;
    if (diff [0] > diff [best])
        best = 0;
    if (diff [2] > diff [best])
        best = 2;

    return best;
}

static int
compare_rgba_0 (gconstpointer a, gconstpointer b)
{
    const guint8 *ab = a;
    const guint8 *bb = b;
    gint ai = ab [0];
    gint bi = bb [0];

    return ai - bi;
}

static int
compare_rgba_1 (gconstpointer a, gconstpointer b)
{
    const guint8 *ab = a;
    const guint8 *bb = b;
    gint ai = ab [1];
    gint bi = bb [1];

    return ai - bi;
}

static int
compare_rgba_2 (gconstpointer a, gconstpointer b)
{
    const guint8 *ab = a;
    const guint8 *bb = b;
    gint ai = ab [2];
    gint bi = bb [2];

    return ai - bi;
}

static int
compare_rgba_3 (gconstpointer a, gconstpointer b)
{
    const guint8 *ab = a;
    const guint8 *bb = b;
    gint ai = ab [3];
    gint bi = bb [3];

    return ai - bi;
}

static void
sort_by_channel (gpointer pixels, gint n_pixels, gint ch)
{
    switch (ch)
    {
        case 0:
            qsort (pixels, n_pixels, sizeof (guint32), compare_rgba_0);
            break;
        case 1:
            qsort (pixels, n_pixels, sizeof (guint32), compare_rgba_1);
            break;
        case 2:
            qsort (pixels, n_pixels, sizeof (guint32), compare_rgba_2);
            break;
        case 3:
            qsort (pixels, n_pixels, sizeof (guint32), compare_rgba_3);
            break;
        default:
            g_assert_not_reached ();
    }
}

#if 0

static void
average_pixels (guint8 *pixels, gint first_ofs, gint n_pixels, ChafaColor *col_out)
{
    guint8 *p = pixels + first_ofs * sizeof (guint32);
    guint8 *pixels_end;
    gint ch [3] = { 0 };

    pixels_end = p + n_pixels * sizeof (guint32);

    for ( ; p < pixels_end; p += 4)
    {
        ch [0] += p [0];
        ch [1] += p [1];
        ch [2] += p [2];
    }

    col_out->ch [0] = (ch [0] + n_pixels / 2) / n_pixels;
    col_out->ch [1] = (ch [1] + n_pixels / 2) / n_pixels;
    col_out->ch [2] = (ch [2] + n_pixels / 2) / n_pixels;
}

#endif

static void
median_pixels (guint8 *pixels, gint first_ofs, gint n_pixels, ChafaColor *col_out)
{
    guint8 *p = pixels + (first_ofs + n_pixels / 2) * sizeof (guint32);
    col_out->ch [0] = p [0];
    col_out->ch [1] = p [1];
    col_out->ch [2] = p [2];
}

static void
average_pixels_weighted_by_deviation (guint8 *pixels, gint first_ofs, gint n_pixels,
                                      ChafaColor *col_out)
{
    guint8 *p = pixels + first_ofs * sizeof (guint32);
    guint8 *pixels_end;
    gint ch [3] = { 0 };
    ChafaColor median;
    guint sum = 0;

    median_pixels (pixels, first_ofs, n_pixels, &median);

    pixels_end = p + n_pixels * sizeof (guint32);

    for ( ; p < pixels_end; p += 4)
    {
        ChafaColor t;
        guint diff;

        t.ch [0] = p [0];
        t.ch [1] = p [1];
        t.ch [2] = p [2];

        diff = chafa_color_diff_fast (&median, &t);
        diff /= 256;
        diff += 1;

        ch [0] += p [0] * diff;
        ch [1] += p [1] * diff;
        ch [2] += p [2] * diff;

        sum += diff;
    }

    col_out->ch [0] = (ch [0] + sum / 2) / sum;
    col_out->ch [1] = (ch [1] + sum / 2) / sum;
    col_out->ch [2] = (ch [2] + sum / 2) / sum;
}

static void
pick_box_color (gpointer pixels, gint first_ofs, gint n_pixels, ChafaColor *color_out)
{
    average_pixels_weighted_by_deviation (pixels, first_ofs, n_pixels, color_out);
}

static void
median_cut_once (gpointer pixels,
                 gint first_ofs, gint n_pixels,
                 ChafaColor *color_out)
{
    guint8 *p = pixels;
    gint dominant_ch;

    g_assert (n_pixels > 0);

    dominant_ch = find_dominant_channel (p + first_ofs * sizeof (guint32), n_pixels);
    sort_by_channel (p + first_ofs * sizeof (guint32), n_pixels, dominant_ch);

    pick_box_color (pixels, first_ofs, n_pixels, color_out);
}

static void
median_cut (ChafaPalette *pal, gpointer pixels,
            gint first_ofs, gint n_pixels,
            gint first_col, gint n_cols)
{
    guint8 *p = pixels;
    gint dominant_ch;

    g_assert (n_pixels > 0);
    g_assert (n_cols > 0);

    dominant_ch = find_dominant_channel (p + first_ofs * sizeof (guint32), n_pixels);
    sort_by_channel (p + first_ofs * sizeof (guint32), n_pixels, dominant_ch);

    if (n_cols == 1 || n_pixels < 2)
    {
        pick_box_color (pixels, first_ofs, n_pixels, &pal->colors [first_col].col [CHAFA_COLOR_SPACE_RGB]);
        return;
    }

    median_cut (pal, pixels,
                first_ofs,
                n_pixels / 2,
                first_col,
                n_cols / 2);

    median_cut (pal, pixels,
                first_ofs + (n_pixels / 2),
                n_pixels - (n_pixels / 2),
                first_col + (n_cols / 2),
                n_cols - (n_cols / 2));
}

static gint
dominant_diff (guint8 *p1, guint8 *p2)
{
    gint diff [3];

    diff [0] = abs (p2 [0] - (gint) p1 [0]);
    diff [1] = abs (p2 [1] - (gint) p1 [1]);
    diff [2] = abs (p2 [2] - (gint) p1 [2]);

    return MAX (diff [0], MAX (diff [1], diff [2]));
}

static void
diversity_pass (ChafaPalette *pal, gpointer pixels,
                gint n_pixels,
                gint first_col, gint n_cols)
{
    guint8 *p = pixels;
    gint step = n_pixels / 128;
    gint i, n, c;
    guint8 done [128] = { 0 };

    step = MAX (step, 1);

    for (c = 0; c < n_cols; c++)
    {
        gint best_box = 0;
        gint best_diff = 0;

        for (i = 0, n = 0; i < 128 && i < n_pixels; i++)
        {
            gint diff = dominant_diff (p + 4 * n, p + 4 * (n + step - 1));

            if (diff > best_diff && !done [i])
            {
                best_diff = diff;
                best_box = i;
            }

            n += step;
        }

        median_cut_once (pixels, best_box * step, MAX (step / 2, 1),
                         &pal->colors [first_col + c].col [CHAFA_COLOR_SPACE_RGB]);
        c++;
        if (c >= n_cols)
            break;

        median_cut_once (pixels, best_box * step + step / 2, MAX (step / 2, 1),
                         &pal->colors [first_col + c].col [CHAFA_COLOR_SPACE_RGB]);

        done [best_box] = 1;
    }
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

#define N_SAMPLES 32768

static gint
extract_samples (gconstpointer pixels, gpointer pixels_out, gint n_pixels, gint step,
                 gint alpha_threshold)
{
    const guint32 *p = pixels;
    guint32 *p_out = pixels_out;
    gint i;

    step = MAX (step, 1);

    for (i = 0; i < n_pixels; i += step)
    {
        gint alpha = p [i] >> 24;
        if (alpha < alpha_threshold)
            continue;
        *(p_out++) = p [i];
    }

    return ((ptrdiff_t) p_out - (ptrdiff_t) pixels_out) / 4;
}

static gint
extract_samples_dense (gconstpointer pixels, gpointer pixels_out, gint n_pixels,
                       gint n_samples_max, gint alpha_threshold)
{
    const guint32 *p = pixels;
    guint32 *p_out = pixels_out;
    gint n_samples = 0;
    gint i;

    g_assert (n_samples_max > 0);

    for (i = 0; i < n_pixels; i++)
    {
        gint alpha = p [i] >> 24;
        if (alpha < alpha_threshold)
            continue;

        *(p_out++) = p [i];

        n_samples++;
        if (n_samples == n_samples_max)
            break;
    }

    return n_samples;
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
                        ChafaColorSpace color_space)
{
    guint32 *pixels_copy;
    gint step;
    gint copy_n_pixels;

    if (palette_out->type != CHAFA_PALETTE_TYPE_DYNAMIC_256)
        return;

    pixels_copy = g_malloc (N_SAMPLES * sizeof (guint32));

    step = (n_pixels / N_SAMPLES) + 1;
    copy_n_pixels = extract_samples (pixels, pixels_copy, n_pixels, step,
                                     palette_out->alpha_threshold);

    /* If we recovered very few (potentially zero) samples, it could be due to
     * the image being mostly transparent. Resample at full density if so. */
    if (copy_n_pixels < 256 && step != 1)
        copy_n_pixels = extract_samples_dense (pixels, pixels_copy, n_pixels, N_SAMPLES,
                                               palette_out->alpha_threshold);

    DEBUG (g_printerr ("Extracted %d samples.\n", copy_n_pixels));

    if (copy_n_pixels < 1)
    {
        palette_out->n_colors = 0;
        goto out;
    }

    median_cut (palette_out, pixels_copy, 0, copy_n_pixels, 0, 128);
    palette_out->n_colors = 128;
    clean_up (palette_out);

    diversity_pass (palette_out, pixels_copy, copy_n_pixels, palette_out->n_colors, 256 - palette_out->n_colors);
    palette_out->n_colors = 256;
    clean_up (palette_out);

    gen_table (palette_out, CHAFA_COLOR_SPACE_RGB);

    if (color_space == CHAFA_COLOR_SPACE_DIN99D)
    {
        gen_din99d_color_space (palette_out);
        gen_table (palette_out, CHAFA_COLOR_SPACE_DIN99D);
    }

out:
    g_free (pixels_copy);
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

