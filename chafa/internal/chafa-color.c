/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2021 Hans Petter Jansson
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
#include <math.h>  /* pow, cbrt, log, sqrt, atan2, cos, sin */
#include "chafa.h"
#include "internal/chafa-color.h"

#define DEBUG(x)

/* 256-color values */
static guint32 term_colors_256 [CHAFA_PALETTE_INDEX_MAX] =
{
    0x000000, 0x800000, 0x007000, 0x707000, 0x000070, 0x700070, 0x007070, 0xc0c0c0,
    /* 0x808080 -> */ 0x404040, 0xff0000, 0x00ff00, 0xffff00, 0x0000ff, 0xff00ff, 0x00ffff, 0xffffff,

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

    0x808080,  /* Transparent */
    0xffffff,  /* Foreground */
    0x000000,  /* Background */
};

static ChafaPaletteColor palette_256 [CHAFA_PALETTE_INDEX_MAX];
static guchar color_cube_216_channel_index [256];
static gboolean palette_initialized;

void
chafa_init_palette (void)
{
    gint i;

    if (palette_initialized)
        return;

    for (i = 0; i < CHAFA_PALETTE_INDEX_MAX; i++)
    {
        chafa_unpack_color (term_colors_256 [i], &palette_256 [i].col [0]);
        chafa_color_rgb_to_din99d (&palette_256 [i].col [0], &palette_256 [i].col [1]);

        palette_256 [i].col [0].ch [3] = 0xff;  /* Fully opaque */
        palette_256 [i].col [1].ch [3] = 0xff;  /* Fully opaque */
    }

    /* Transparent color */
    palette_256 [CHAFA_PALETTE_INDEX_TRANSPARENT].col [0].ch [3] = 0x00;
    palette_256 [CHAFA_PALETTE_INDEX_TRANSPARENT].col [1].ch [3] = 0x00;

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

const ChafaColor *
chafa_get_palette_color_256 (guint index, ChafaColorSpace color_space)
{
    return &palette_256 [index].col [color_space];
}

guint32
chafa_pack_color (const ChafaColor *color)
{
    /* Assumes each channel 0 <= value <= 255 */
    return (color->ch [0] << 16)
            | (color->ch [1] << 8)
            | (color->ch [2])
            | (color->ch [3] << 24);  /* Alpha */
}

void
chafa_unpack_color (guint32 packed, ChafaColor *color_out)
{
    color_out->ch [0] = (packed >> 16) & 0xff;
    color_out->ch [1] = (packed >> 8) & 0xff;
    color_out->ch [2] = packed & 0xff;
    color_out->ch [3] = (packed >> 24) & 0xff;  /* Alpha */
}

void
chafa_color_accum_div_scalar (ChafaColorAccum *accum, gint scalar)
{
    accum->ch [0] /= scalar;
    accum->ch [1] /= scalar;
    accum->ch [2] /= scalar;
    accum->ch [3] /= scalar;
}

typedef struct
{
    gdouble c [3];
}
ChafaColorRGBf;

typedef struct
{
    gdouble c [3];
}
ChafaColorXYZ;

typedef struct
{
    gdouble c [3];
}
ChafaColorLab;

static gdouble
invert_rgb_channel_compand (gdouble v)
{
    return v <= 0.04045 ? (v / 12.92) : pow ((v + 0.055) / 1.044, 2.4);
}

static void
convert_rgb_to_xyz (const ChafaColor *rgbi, ChafaColorXYZ *xyz)
{
    ChafaColorRGBf rgbf;
    gint i;

    for (i = 0; i < 3; i++)
    {
        rgbf.c [i] = (gdouble) rgbi->ch [i] / 255.0;
        rgbf.c [i] = invert_rgb_channel_compand (rgbf.c [i]);
    }

    xyz->c [0] = 0.4124564 * rgbf.c [0] + 0.3575761 * rgbf.c [1] + 0.1804375 * rgbf.c [2];
    xyz->c [1] = 0.2126729 * rgbf.c [0] + 0.7151522 * rgbf.c [1] + 0.0721750 * rgbf.c [2];
    xyz->c [2] = 0.0193339 * rgbf.c [0] + 0.1191920 * rgbf.c [1] + 0.9503041 * rgbf.c [2];
}

#define XYZ_EPSILON (216.0 / 24389.0)
#define XYZ_KAPPA (24389.0 / 27.0)

static gdouble
lab_f (gdouble v)
{
    return v > XYZ_EPSILON ? cbrt (v) : (XYZ_KAPPA * v + 16.0) / 116.0;
}

static void
convert_xyz_to_lab (const ChafaColorXYZ *xyz, ChafaColorLab *lab)
{
    ChafaColorXYZ wp = { { 0.95047, 1.0, 1.08883 } };  /* D65 white point */
    ChafaColorXYZ xyz2;
    gint i;

    for (i = 0; i < 3; i++)
        xyz2.c [i] = lab_f (xyz->c [i] / wp.c [i]);

    lab->c [0] = 116.0 * xyz2.c [1] - 16.0;
    lab->c [1] = 500.0 * (xyz2.c [0] - xyz2.c [1]);
    lab->c [2] = 200.0 * (xyz2.c [1] - xyz2.c [2]);
}

void
chafa_color_rgb_to_din99d (const ChafaColor *rgb, ChafaColor *din99)
{
    ChafaColorXYZ xyz;
    ChafaColorLab lab;
    gdouble adj_L, ee, f, G, C, h;

    convert_rgb_to_xyz (rgb, &xyz);

    /* Apply tristimulus-space correction term */

    xyz.c [0] = 1.12 * xyz.c [0] - 0.12 * xyz.c [2];

    /* Convert to L*a*b* */

    convert_xyz_to_lab (&xyz, &lab);
    adj_L = 325.22 * log (1.0 + 0.0036 * lab.c [0]);

    /* Intermediate parameters */

    ee = 0.6427876096865393 * lab.c [1] + 0.766044443118978 * lab.c [2];
    f = 1.14 * (0.6427876096865393 * lab.c [2] - 0.766044443118978 * lab.c [1]);
    G = sqrt (ee * ee + f * f);

    /* Hue/chroma */

    C = 22.5 * log (1.0 + 0.06 * G);

    h = atan2 (f, ee) + 0.8726646 /* 50 degrees */;
    while (h < 0.0) h += 6.283185;  /* 360 degrees */
    while (h > 6.283185) h -= 6.283185;  /* 360 degrees */

    /* The final values should be in the range [0..255] */

    din99->ch [0] = adj_L * 2.5;
    din99->ch [1] = C * cos (h) * 2.5 + 128.0;
    din99->ch [2] = C * sin (h) * 2.5 + 128.0;
    din99->ch [3] = rgb->ch [3];
}

static gint
color_diff_rgb (const ChafaColor *col_a, const ChafaColor *col_b)
{
    gint error = 0;
    gint d [3];

    d [0] = (gint) col_b->ch [0] - (gint) col_a->ch [0];
    d [0] = d [0] * d [0];
    d [1] = (gint) col_b->ch [1] - (gint) col_a->ch [1];
    d [1] = d [1] * d [1];
    d [2] = (gint) col_b->ch [2] - (gint) col_a->ch [2];
    d [2] = d [2] * d [2];

    error = 2 * d [0] + 4 * d [1] + 3 * d [2]
    + (((col_a->ch [0] + (gint) col_b->ch [0]) / 2)
        * abs (d [0] - d [2])) / 256;

    return error;
}

static gint
color_diff_euclidean (const ChafaColor *col_a, const ChafaColor *col_b)
{
    gint error = 0;
    gint d [3];

    d [0] = (gint) col_b->ch [0] - (gint) col_a->ch [0];
    d [0] = d [0] * d [0];
    d [1] = (gint) col_b->ch [1] - (gint) col_a->ch [1];
    d [1] = d [1] * d [1];
    d [2] = (gint) col_b->ch [2] - (gint) col_a->ch [2];
    d [2] = d [2] * d [2];

    error = d [0] + d [1] + d [2];
    return error;
}

static gint
color_diff_alpha (const ChafaColor *col_a, const ChafaColor *col_b, gint error)
{
    gint max_opacity;
    gint a;

    /* Alpha */
    a = (gint) col_b->ch [3] - (gint) col_a->ch [3];
    a = a * a;
    max_opacity = MAX (col_a->ch [3], col_b->ch [3]);
    error *= max_opacity;
    error /= 256;
    error += a * 8;

    return error;
}

gint
chafa_color_diff_slow (const ChafaColor *col_a, const ChafaColor *col_b, ChafaColorSpace color_space)
{
    gint error;

    if (color_space == CHAFA_COLOR_SPACE_RGB)
        error = color_diff_rgb (col_a, col_b);
    else if (color_space == CHAFA_COLOR_SPACE_DIN99D)
        error = color_diff_euclidean (col_a, col_b);
    else
    {
        g_assert_not_reached ();
        return -1;
    }

    error = color_diff_alpha (col_a, col_b, error);

    return error;
}

/* FIXME: We may be able to avoid mixing alpha in most cases, but 16-color fill relies
 * on it at the moment. */
void
chafa_color_mix (ChafaColor *out, const ChafaColor *a, const ChafaColor *b, gint ratio)
{
    gint i;

    for (i = 0; i < 4; i++)
        out->ch [i] = (a->ch [i] * ratio + b->ch [i] * (1000 - ratio)) / 1000;
}

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

static gint
update_candidates_with_color_index_diff (ChafaColorCandidates *candidates, ChafaColorSpace color_space, const ChafaColor *color, gint index)
{
    const ChafaColor *palette_color;
    gint error;

    palette_color = chafa_get_palette_color_256 (index, color_space);
    error = chafa_color_diff_slow (color, palette_color, color_space);
    update_candidates (candidates, index, error);

    return error;
}

static void
pick_color_216_cube (const ChafaColor *color, ChafaColorSpace color_space, ChafaColorCandidates *candidates)
{
    gint i;

    g_assert (color_space == CHAFA_COLOR_SPACE_RGB);

    i = 16 + (color_cube_216_channel_index [color->ch [0]] * 6 * 6
              + color_cube_216_channel_index [color->ch [1]] * 6
              + color_cube_216_channel_index [color->ch [2]]);

    update_candidates_with_color_index_diff (candidates, color_space, color, i);
}

static void
pick_color_24_grays (const ChafaColor *color, ChafaColorSpace color_space, ChafaColorCandidates *candidates)
{
    const ChafaColor *palette_color;
    gint error, last_error = G_MAXINT;
    gint step, i;

    g_assert (color_space == CHAFA_COLOR_SPACE_RGB);

    i = 232 + 12;
    last_error = update_candidates_with_color_index_diff (candidates, color_space, color, i);

    palette_color = chafa_get_palette_color_256 (i + 1, color_space);
    error = chafa_color_diff_slow (color, palette_color, color_space);
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
        palette_color = chafa_get_palette_color_256 (i, color_space);

        error = chafa_color_diff_slow (color, palette_color, color_space);
        if (error > last_error)
            break;

        update_candidates (candidates, i, error);
        last_error = error;
    }
    while (i >= 232 && i <= 255);
}

static void
pick_color_16 (const ChafaColor *color, ChafaColorSpace color_space, ChafaColorCandidates *candidates)
{
    gint i;

    for (i = 0; i < 16; i++)
    {
        update_candidates_with_color_index_diff (candidates, color_space, color, i);
    }

    /* Try transparency */

    update_candidates_with_color_index_diff (candidates, color_space, color,
                                             CHAFA_PALETTE_INDEX_TRANSPARENT);
}

void
chafa_pick_color_16 (const ChafaColor *color, ChafaColorSpace color_space, ChafaColorCandidates *candidates)
{
    init_candidates (candidates);
    pick_color_16 (color, color_space, candidates);
}


static void
pick_color_8 (const ChafaColor *color, ChafaColorSpace color_space, ChafaColorCandidates *candidates)
{
    gint i;

    for (i = 0; i < 8; i++)
    {
        update_candidates_with_color_index_diff (candidates, color_space, color, i);
    }
#if 0
    /* Try transparency */

    update_candidates_with_color_index_diff (candidates, color_space, color,
                                             CHAFA_PALETTE_INDEX_TRANSPARENT);
#endif
}

void
chafa_pick_color_8 (const ChafaColor *color, ChafaColorSpace color_space, ChafaColorCandidates *candidates)
{
    init_candidates (candidates);
    pick_color_8 (color, color_space, candidates);
}


void
chafa_pick_color_256 (const ChafaColor *color, ChafaColorSpace color_space, ChafaColorCandidates *candidates)
{
    gint i;

    init_candidates (candidates);

    if (color_space == CHAFA_COLOR_SPACE_RGB)
    {
        pick_color_216_cube (color, color_space, candidates);
        pick_color_24_grays (color, color_space, candidates);

        /* This will try transparency too. Do this last so ties are broken in
         * favor of high-index colors. */
        pick_color_16 (color, color_space, candidates);
    }
    else
    {
        /* All colors including transparent, but not bg or fg */
        for (i = 0; i < 257; i++)
        {
            update_candidates_with_color_index_diff (candidates, color_space, color, i);
        }
    }
}

void
chafa_pick_color_240 (const ChafaColor *color, ChafaColorSpace color_space, ChafaColorCandidates *candidates)
{
    gint i;

    init_candidates (candidates);

    if (color_space == CHAFA_COLOR_SPACE_RGB)
    {
        pick_color_216_cube (color, color_space, candidates);
        pick_color_24_grays (color, color_space, candidates);

        /* Try transparency */

        update_candidates_with_color_index_diff (candidates, color_space, color,
                                                 CHAFA_PALETTE_INDEX_TRANSPARENT);
    }
    else
    {
        /* Color cube and transparent, but not lower 16, bg or fg */
        for (i = 16; i < 257; i++)
        {
            update_candidates_with_color_index_diff (candidates, color_space, color, i);
        }
    }
}

/* Pick the best approximation of color from a palette consisting of
 * fg_color and bg_color */
void
chafa_pick_color_fgbg (const ChafaColor *color, ChafaColorSpace color_space,
                       const ChafaColor *fg_color, const ChafaColor *bg_color,
                       ChafaColorCandidates *candidates)
{
    gint error;

    init_candidates (candidates);

    error = chafa_color_diff_slow (color, fg_color, color_space);
    update_candidates (candidates, CHAFA_PALETTE_INDEX_FG, error);

    error = chafa_color_diff_slow (color, bg_color, color_space);
    update_candidates (candidates, CHAFA_PALETTE_INDEX_BG, error);

    /* Consider opaque background too */

    if (candidates->index [0] != CHAFA_PALETTE_INDEX_BG)
    {
        ChafaColor bg_color_opaque = *bg_color;
        bg_color_opaque.ch [3] = 0xff;

        error = chafa_color_diff_slow (color, &bg_color_opaque, color_space);
        update_candidates (candidates, CHAFA_PALETTE_INDEX_BG, error);
    }
}
