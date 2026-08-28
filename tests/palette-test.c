/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2026 Hans Petter Jansson
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

#include <chafa.h>
#include "internal/chafa-palette.h"
#include <stdio.h>
#include <string.h>

/* chafa_palette_channel_to_level() and chafa_palette_level_to_channel() must
 * round-trip exactly for every level count, and snapping must be idempotent
 * and never move a value by more than half a step (plus rounding). */
static void
level_round_trip_test (void)
{
    gint n_levels, level, ch;

    for (n_levels = 2; n_levels <= 256; n_levels++)
    {
        for (level = 0; level < n_levels; level++)
        {
            ch = chafa_color_level_to_channel (level, n_levels);

            g_assert_cmpint (ch, >=, 0);
            g_assert_cmpint (ch, <=, 255);
            g_assert_cmpint (chafa_color_channel_to_level (ch, n_levels), ==, level);
        }

        for (ch = 0; ch < 256; ch++)
        {
            gint snapped, resnapped;

            level = chafa_color_channel_to_level (ch, n_levels);
            g_assert_cmpint (level, >=, 0);
            g_assert_cmpint (level, <, n_levels);

            snapped = chafa_color_level_to_channel (level, n_levels);
            resnapped = chafa_color_level_to_channel (chafa_color_channel_to_level (snapped, n_levels),
                                                      n_levels);

            /* |snapped - ch| <= 255 / (2 * (n_levels - 1)) + 1/2 */
            g_assert_cmpint (resnapped, ==, snapped);
            g_assert_cmpint (ABS (snapped - ch) * 2 * (n_levels - 1), <=, 255 + (n_levels - 1));
            if (n_levels == 256)
                g_assert_cmpint (snapped, ==, ch);
        }
    }
}

static gboolean
color_is_snapped (const ChafaPalette *palette, const ChafaColor *col)
{
    ChafaColor snapped = chafa_palette_snap_color (palette, *col);

    return chafa_color8_to_u32 (snapped) == chafa_color8_to_u32 (*col);
}

/* Every pen's RGB color must be on the snapped channel grid, its DIN99d color
 * must be derived from that, and looking up a pen's own color in color_space
 * must yield a pen with that color. */
static void
assert_palette_is_consistent (const ChafaPalette *palette, ChafaColorSpace color_space)
{
    gint first_color = chafa_palette_get_first_color (palette);
    gint i;

    for (i = first_color; i < first_color + chafa_palette_get_n_colors (palette); i++)
    {
        const ChafaColor *rgb;
        const ChafaColor *din99d;
        const ChafaColor *col;
        const ChafaColor *found;
        ChafaColor derived;
        gint index;

        if (i == chafa_palette_get_transparent_index (palette))
            continue;

        rgb = chafa_palette_get_color (palette, CHAFA_COLOR_SPACE_RGB, i);
        din99d = chafa_palette_get_color (palette, CHAFA_COLOR_SPACE_DIN99D, i);

        g_assert_true (color_is_snapped (palette, rgb));

        chafa_color_rgb_to_din99d (rgb, &derived);
        g_assert_cmpuint (chafa_color8_to_u32 (*din99d), ==, chafa_color8_to_u32 (derived));

        col = color_space == CHAFA_COLOR_SPACE_RGB ? rgb : din99d;
        index = chafa_palette_lookup_nearest (palette, color_space, col, NULL);
        found = chafa_palette_get_color (palette, color_space, index);

        g_assert_cmpuint (chafa_color8_to_u32 (*found), ==, chafa_color8_to_u32 (*col));
    }
}

static void
palette_snap_test (void)
{
    ChafaPalette palette;
    ChafaColor col = { { 1, 2, 3, 255 } };
    const ChafaColor *got;
    guint8 *pixels;
    gint n_pixels = 64 * 64;
    gint i;

    chafa_palette_init (&palette, CHAFA_PALETTE_TYPE_DYNAMIC_256);
    chafa_palette_set_alpha_threshold (&palette, 127);
    g_assert_cmpint (chafa_palette_get_channel_levels (&palette), ==, 256);

    /* Colors set explicitly get snapped: with 101 levels, 1 -> 0 and 2, 3 -> 3 */

    chafa_palette_set_channel_levels (&palette, 101);
    chafa_palette_set_color (&palette, 0, &col);
    got = chafa_palette_get_color (&palette, CHAFA_COLOR_SPACE_RGB, 0);

    g_assert_cmpint (got->ch [0], ==, 0);
    g_assert_cmpint (got->ch [1], ==, 3);
    g_assert_cmpint (got->ch [2], ==, 3);
    g_assert_cmpint (got->ch [3], ==, 255);

    /* Generated palettes are on the snapped grid and self-consistent */

    pixels = g_malloc (n_pixels * 4);
    for (i = 0; i < n_pixels; i++)
    {
        pixels [i * 4 + 0] = i & 0xff;
        pixels [i * 4 + 1] = (i >> 4) & 0xff;
        pixels [i * 4 + 2] = (i * 7) & 0xff;
        pixels [i * 4 + 3] = 0xff;
    }

    chafa_palette_generate (&palette, pixels, n_pixels, CHAFA_COLOR_SPACE_RGB, 1.0f);
    assert_palette_is_consistent (&palette, CHAFA_COLOR_SPACE_RGB);

    /* Changing the resolution afterwards re-snaps colors and rebuilds tables */

    chafa_palette_set_channel_levels (&palette, 33);
    assert_palette_is_consistent (&palette, CHAFA_COLOR_SPACE_RGB);

    chafa_palette_set_channel_levels (&palette, 256);
    assert_palette_is_consistent (&palette, CHAFA_COLOR_SPACE_RGB);

    /* Same again in DIN99d, which has its own lookup table */

    chafa_palette_deinit (&palette);

    chafa_palette_init (&palette, CHAFA_PALETTE_TYPE_DYNAMIC_256);
    chafa_palette_set_alpha_threshold (&palette, 127);
    chafa_palette_set_channel_levels (&palette, 101);

    chafa_palette_generate (&palette, pixels, n_pixels, CHAFA_COLOR_SPACE_DIN99D, 1.0f);
    assert_palette_is_consistent (&palette, CHAFA_COLOR_SPACE_DIN99D);

    chafa_palette_set_channel_levels (&palette, 33);
    assert_palette_is_consistent (&palette, CHAFA_COLOR_SPACE_DIN99D);

    g_free (pixels);
    chafa_palette_deinit (&palette);
}

int
main (int argc, char *argv [])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/palette/level-round-trip", level_round_trip_test);
    g_test_add_func ("/palette/snap", palette_snap_test);

    return g_test_run ();
}
