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
#include "internal/chafa-pca.h"

#define POW2(x) ((x) * (x))

#define FIXED_MUL 1
#define FIXED_MUL_F ((gfloat) FIXED_MUL)

/* Number of colors to compare against outside the strict 2D range defined
 * by the PC vectors. This partly makes up for suboptimal choices resulting
 * from dimensionality reduction and imprecision.
 *
 * The final number of extra probes is twice this value -- once above and
 * once below the initial range. */

#define N_BLIND_PROBES 2

static gint
compare_entries (gconstpointer a, gconstpointer b)
{
    const ChafaColorTableEntry *ab = a;
    const ChafaColorTableEntry *bb = b;

    return ab->v [0] - bb->v [0];
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

static void
project_color (const ChafaColorTable *color_table, guint32 color, gint *v_out)
{
    ChafaVec3i32 v;

    /* FIXME: We may avoid the * FIXED_MUL both here and in average, but only
     * if that leaves a precise enough average. */

    v.v [0] = (color & 0xff) * FIXED_MUL;
    v.v [1] = ((color >> 8) & 0xff) * FIXED_MUL;
    v.v [2] = ((color >> 16) & 0xff) * FIXED_MUL;

    chafa_vec3i32_sub (&v, &v, &color_table->average);

    v_out [0] = chafa_vec3i32_dot_32 (&v, &color_table->eigenvectors [0]);
    v_out [1] = chafa_vec3i32_dot_32 (&v, &color_table->eigenvectors [1]);
}

static void
vec3i32_fixed_point_from_vec3f32 (ChafaVec3i32 *out, const ChafaVec3f32 *in)
{
    ChafaVec3f32 t;

    chafa_vec3f32_mul_scalar (&t, in, FIXED_MUL_F);
    chafa_vec3i32_from_vec3f32 (out, &t);
}

static void
do_pca (ChafaColorTable *color_table)
{
    ChafaVec3f32 v [256];
    ChafaVec3f32 eigenvectors [2];
    ChafaVec3f32 average;
    gint i, j;

    for (i = 0, j = 0; i < 256; i++)
    {
        guint32 col = color_table->pens [i];

        if ((col & 0xff000000) == 0xff000000)
            continue;

        v [j].v [0] = (col & 0xff) * FIXED_MUL_F;
        v [j].v [1] = ((col >> 8) & 0xff) * FIXED_MUL_F;
        v [j].v [2] = ((col >> 16) & 0xff) * FIXED_MUL_F;
        j++;
    }

    chafa_vec3f32_array_compute_pca (v, j,
                                     2,
                                     eigenvectors,
                                     NULL,
                                     &average);

    chafa_vec3i32_from_vec3f32 (&color_table->eigenvectors [0], &eigenvectors [0]);
    chafa_vec3i32_from_vec3f32 (&color_table->eigenvectors [1], &eigenvectors [1]);
    chafa_vec3i32_from_vec3f32 (&color_table->average, &average);

    for (i = 0; i < color_table->n_entries; i++)
    {
        ChafaColorTableEntry *entry = &color_table->entries [i];
        project_color (color_table, color_table->pens [entry->pen], entry->v);
    }
}

static inline gboolean
refine_pen_choice (const ChafaColorTable *color_table, guint want_color, const gint *v, gint j,
                   gint *best_pen, gint *best_diff)
{
    const ChafaColorTableEntry *pj = &color_table->entries [j];
    gint a, b, d;

    a = POW2 (pj->v [0] - v [0]);

    if (a <= *best_diff)
    {
        b = POW2 (pj->v [1] - v [1]);

        if (b <= *best_diff)
        {
            d = a + b;

            if (d <= *best_diff)
            {
                if (color_diff (color_table->pens [pj->pen], want_color) <
                    color_diff (color_table->pens [color_table->entries [*best_pen].pen], want_color))
                {
                    *best_pen = j;
                    *best_diff = d;
                }
            }
        }
    }
    else
    {
        return FALSE;
    }

    return TRUE;
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
        ChafaColorTableEntry *entry;

        if (color_table->pens [i] == 0xffffffff)
            continue;

        entry = &color_table->entries [j++];
        entry->pen = i;
    }

    color_table->n_entries = j;

    do_pca (color_table);

    qsort (color_table->entries, color_table->n_entries, sizeof (ChafaColorTableEntry), compare_entries);
    color_table->is_sorted = TRUE;
}

gint
chafa_color_table_find_nearest_pen (const ChafaColorTable *color_table, guint32 want_color)
{
    gint best_diff = G_MAXINT;
    gint best_pen = 0;
    gint v [2];
    gint i, j, m;

    g_assert (color_table->n_entries > 0);
    g_assert (color_table->is_sorted);

    project_color (color_table, want_color, v);

    /* Binary search for first vector component */

    i = 0;
    j = color_table->n_entries;

    while (i != j)
    {
        gint n = i + (j - i) / 2;

        if (v [0] > color_table->entries [n].v [0])
            i = n + 1;
        else
            j = n;
    }

    m = j;

    /* Left scan for closer match */

    for (j = m; j >= 0; j--)
    {
        if (!refine_pen_choice (color_table, want_color, v, j, &best_pen, &best_diff))
            break;
    }

    for (i = 0; i < N_BLIND_PROBES && j > 0; i++)
    {
        j--;
        if (color_diff (color_table->pens [color_table->entries [j].pen], want_color) <
            color_diff (color_table->pens [color_table->entries [best_pen].pen], want_color))
        {
            const ChafaColorTableEntry *pj = &color_table->entries [j];

            best_pen = j;
            best_diff = POW2 (pj->v [0] - v [0]) + POW2 (pj->v [1] - v [1]);
        }
    }

    /* Right scan for closer match */

    for (j = m + 1; j < color_table->n_entries; j++)
    {
        if (!refine_pen_choice (color_table, want_color, v, j, &best_pen, &best_diff))
            break;
    }

    for (i = 0; i < N_BLIND_PROBES && j < color_table->n_entries - 1; i++)
    {
        j++;
        if (color_diff (color_table->pens [color_table->entries [j].pen], want_color) <
            color_diff (color_table->pens [color_table->entries [best_pen].pen], want_color))
        {
            const ChafaColorTableEntry *pj = &color_table->entries [j];

            best_pen = j;
            best_diff = POW2 (pj->v [0] - v [0]) + POW2 (pj->v [1] - v [1]);
        }
    }

    return color_table->entries [best_pen].pen;
}
