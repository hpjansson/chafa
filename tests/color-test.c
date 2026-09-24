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
#include "internal/chafa-color.h"

/* Accept some drift due to FP optimizations (ours and compiler's) */
#define DIN99D_CH_TOLERANCE 2

/* Grays have no chroma, and their lightness is monotone. Black is 0 and white
 * is 100 on DIN99d's lightness scale, scaled to [0..250] in our implementation. */
static void
din99d_neutral_axis_test (void)
{
    gint prev = -1;
    gint i;

    for (i = 0; i < 256; i++)
    {
        ChafaColor rgb = { { (guint8) i, (guint8) i, (guint8) i, 255 } };
        ChafaColor din99;

        chafa_color_rgb_to_din99d (&rgb, &din99);

        g_assert_cmpint (din99.ch [0], >=, prev - DIN99D_CH_TOLERANCE);
        g_assert_cmpint (din99.ch [1], ==, 128);
        g_assert_cmpint (din99.ch [2], ==, 128);
        g_assert_cmpint (din99.ch [3], ==, 255);

        if (i == 0)
            g_assert_cmpint (din99.ch [0], <=, DIN99D_CH_TOLERANCE);
        else if (i == 255)
            g_assert_cmpint (din99.ch [0], >=, 250 - DIN99D_CH_TOLERANCE);

        prev = din99.ch [0];
    }
}

/* Spot checks against precomputed values */
static void
din99d_reference_test (void)
{
    static const struct
    {
        guint8 rgb [3];
        guint8 din99 [3];
    }
    cases [] =
    {
        { { 255,   0,   0 }, { 143, 227, 192 } },
        { {   0, 255,   0 }, { 223,  46, 217 } },
        { {   0,   0, 255 }, {  89, 156,  14 } },
        { { 255, 255,   0 }, { 244, 116, 237 } },
        { {   0, 255, 255 }, { 231,  38, 114 } },
        { { 255,   0, 255 }, { 160, 226,  60 } },
        { {  51,  51,  51 }, {  60, 128, 128 } },
        { { 204, 204, 204 }, { 210, 128, 128 } }
    };
    guint i;

    for (i = 0; i < G_N_ELEMENTS (cases); i++)
    {
        ChafaColor rgb = { { cases [i].rgb [0], cases [i].rgb [1], cases [i].rgb [2], 77 } };
        ChafaColor din99;
        gint j;

        chafa_color_rgb_to_din99d (&rgb, &din99);

        for (j = 0; j < 3; j++)
        {
            g_assert_cmpint (ABS ((gint) din99.ch [j] - (gint) cases [i].din99 [j]),
                             <=, DIN99D_CH_TOLERANCE);
        }

        g_assert_cmpint (din99.ch [3], ==, 77);
    }
}

int
main (int argc, char *argv [])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/color/din99d/neutral-axis", din99d_neutral_axis_test);
    g_test_add_func ("/color/din99d/reference", din99d_reference_test);

    return g_test_run ();
}
