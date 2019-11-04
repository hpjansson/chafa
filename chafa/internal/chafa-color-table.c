/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2019 Hans Petter Jansson
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

#include "chafa.h"
#include "internal/chafa-color-table.h"

/* Interleave with one zero:
 *
 * 1 2 3 4 5 6 7 8 
 *   ^ (self << 4) =>
 * 1^5 2^6 3^7 4^8 5 6 7 8 
 *   & 0x0f0f =>
 * x x x x 5 6 7 8 
 *   ^ (self << 2) =>
 * x x 5 6 5^7 6^8 7 8 
 *   & 0x3333 =>
 * x x 5 6 x x 7 8 
 *   ^ (self << 1) =>
 * x 5 5^6 6 x 7 7^8 8 
 *   & 0x5555 =>
 * x 5 x 6 x 7 x 8 
 *
 * Interleave with two zeros: 
 *
 * 1 2 3 4 5 6 7 8 
 *   ^ (self << 8) =>
 * 1 2 3 4 5 6 7 8 
 *   & 0xf00f =>
 * x x x x 5 6 7 8 
 *   ^ (self << 4) =>
 * 5 6 7 8 5 6 7 8 
 *   & 0x30c3 =>
 * 5 6 x x x x 7 8 
 *   ^ (self << 2) =>
 * 5 6 x x 7 8 7 8 
 *   & 0x9249 =>
 * x 6 x x 7 x x 8
 */

#if 0
static uint64_t
interleave_u32_with_0 (uint32_t input)
{
    uint64_t word = input;
    word = (word ^ (word << 16)) & 0x0000ffff0000ffff;
    word = (word ^ (word << 8 )) & 0x00ff00ff00ff00ff;
    word = (word ^ (word << 4 )) & 0x0f0f0f0f0f0f0f0f;
    word = (word ^ (word << 2 )) & 0x3333333333333333;
    word = (word ^ (word << 1 )) & 0x5555555555555555;
    return word;
}
#endif

static guint32
interleave_u8_with_00 (guint8 input)
{
    guint32 word = input;
    word = (word ^ (word << 8)) & 0xf00f;
    word = (word ^ (word << 4)) & 0xc30c3;
    word = (word ^ (word << 2)) & 0x249249;
    return word;
}

/* Channels are presumed to be in ABGR order */
static guint32
interleave_3_channels (guint32 color)
{
    /* Interleaving produces the following bit order:
     * xxxxxxxxGRBGRBGRBGRBGRBGRBGRBGRB (lower 24 bits) */
    return interleave_u8_with_00 (color >> 16)
        | (interleave_u8_with_00 (color) << 1)
        | (interleave_u8_with_00 (color >> 8) << 2);
}

static gint
compare_entries (gconstpointer a, gconstpointer b)
{
    const guint32 *ab = a;
    const guint32 *bb = b;
    gint ai = *ab >> 8;
    gint bi = *bb >> 8;

    return ai - bi;
}

static gint
color_diff (guint32 a, guint32 b)
{
    gint diff;
    gint n;

    n = (gint) (a & 0xff) - (gint) (b & 0xff);
    diff = n * n;
    n = (gint) ((a >> 8) & 0xff) - (gint) ((b >> 8) & 0xff);
    diff += n * n;
    n = (gint) ((a >> 16) & 0xff) - (gint) ((b >> 16) & 0xff);
    diff += n * n;

    return diff;
}

void
chafa_color_table_init (ChafaColorTable *color_table)
{
    color_table->n_entries = 0;
    color_table->is_sorted = TRUE;

    memset (color_table->pens, 0xff, sizeof (color_table->pens));
}

void
chafa_color_table_deinit (G_GNUC_UNUSED ChafaColorTable *color_table)
{
}

guint32
chafa_color_table_get_pen_color (const ChafaColorTable *color_table, gint pen)
{
    g_assert (pen >= 0);
    g_assert (pen < CHAFA_COLOR_TABLE_MAX_ENTRIES);

    return color_table->pens [pen];
}

void
chafa_color_table_set_pen_color (ChafaColorTable *color_table, gint pen, guint32 color)
{
    g_assert (pen >= 0);
    g_assert (pen < CHAFA_COLOR_TABLE_MAX_ENTRIES);

    color_table->pens [pen] = color & 0x00ffffff;
    color_table->is_sorted = FALSE;
}

void
chafa_color_table_sort (ChafaColorTable *color_table)
{
    gint i, j;

    if (color_table->is_sorted)
        return;

    for (i = 0, j = 0; i < CHAFA_COLOR_TABLE_MAX_ENTRIES; i++)
    {
        if (color_table->pens [i] == 0xffffffff)
            continue;

        color_table->entries [j++] = (interleave_3_channels (color_table->pens [i]) << 8) | i;
    }

    color_table->n_entries = j;

    qsort (color_table->entries, color_table->n_entries, sizeof (guint32), compare_entries);
    color_table->is_sorted = TRUE;
}

#define N_COMPARES 4

gint
chafa_color_table_find_nearest_pen (const ChafaColorTable *color_table, guint32 color)
{
    guint32 index;
    gint best_diff = G_MAXINT;
    gint best_pen = 0;
    gint i, j;

    g_assert (color_table->n_entries > 0);
    g_assert (color_table->is_sorted);

    index = interleave_3_channels (color);

    i = 0;
    j = color_table->n_entries;

    while (i != j)
    {
        gint n = i + (j - i) / 2;
        guint32 tindex = color_table->entries [n] >> 8;

        if (index > tindex)
            i = n + 1;
        else
            j = n;
    }

    i = i - N_COMPARES / 2;
    i = MAX (i, 0);

    j = i + N_COMPARES + 1;
    j = MIN (j, color_table->n_entries);

    while (i < j)
    {
        gint diff = color_diff (color, color_table->pens [color_table->entries [i] & 0xff]);
        if (diff < best_diff)
        {
            best_diff = diff;
            best_pen = i;
        }

        i++;
    }

    return color_table->entries [best_pen] & 0xff;
}
