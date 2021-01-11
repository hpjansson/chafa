/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2019-2021 Hans Petter Jansson
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

#define CHAFA_COLOR_TABLE_ENABLE_PROFILING 0
#define DEBUG_PEN_CHOICE(x)

#define POW2(x) ((x) * (x))

#define FIXED_MUL_BIG_SHIFT 14
#define FIXED_MUL_BIG (1 << (FIXED_MUL_BIG_SHIFT))

#define FIXED_MUL 32
#define FIXED_MUL_F ((gfloat) (FIXED_MUL))

#if CHAFA_COLOR_TABLE_ENABLE_PROFILING

# define profile_counter_inc(x) g_atomic_int_inc ((gint *) &(x))

static gint n_lookups;
static gint n_misses;
static gint n_a;
static gint n_b;
static gint n_c;
static gint n_d;

static void
dump_entry (const ChafaColorTable *color_table, gint entry, gint want_color)
{
    const ChafaColorTableEntry *e = &color_table->entries [entry];
    guint32 c = color_table->pens [e->pen];
    gint err;

    err = POW2 (((gint) (c & 0xff) - (want_color & 0xff)))
        + POW2 (((gint) (c >> 8) & 0xff) - ((want_color >> 8) & 0xff))
        + POW2 (((gint) (c >> 16) & 0xff) - ((want_color >> 16) & 0xff));

    g_printerr ("{ %3d, %3d, %3d }  { %7d, %7d }  n = %3d  err = %6d\n",
                c & 0xff,
                (c >> 8) & 0xff,
                (c >> 16) & 0xff,
                e->v [0], e->v [1],
                entry,
                err);
}

#else
# define profile_counter_inc(x)
#endif

static gint
compare_entries (gconstpointer a, gconstpointer b)
{
    const ChafaColorTableEntry *ab = a;
    const ChafaColorTableEntry *bb = b;

    return ab->v [0] - bb->v [0];
}

static gint
scalar_project_vec3i32 (const ChafaVec3i32 *a, const ChafaVec3i32 *b, guint32 b_mul)
{
    guint64 d = chafa_vec3i32_dot_64 (a, b);

    /* I replaced the following (a division, three multiplications and
     * two additions) with a multiplication and a right shift:
     * 
     * d / (POW2 (b->v [0]) + POW2 (b->v [1]) + POW2 (b->v [2]))
     *
     * The result is multiplied by FIXED_MUL for increased precision. */

    return (d * b_mul) / (FIXED_MUL_BIG / FIXED_MUL);
}

static gint
color_diff (guint32 a, guint32 b)
{
    gint diff;
    gint n;

    n = (gint) ((b & 0xff) - (gint) (a & 0xff)) * FIXED_MUL;
    diff = n * n;
    n = (gint) (((b >> 8) & 0xff) - (gint) ((a >> 8) & 0xff)) * FIXED_MUL;
    diff += n * n;
    n = (gint) (((b >> 16) & 0xff) - (gint) ((a >> 16) & 0xff)) * FIXED_MUL;
    diff += n * n;

    return diff;
}

static void
project_color (const ChafaColorTable *color_table, guint32 color, gint *v_out)
{
    ChafaVec3i32 v;

    v.v [0] = (color & 0xff) * FIXED_MUL;
    v.v [1] = ((color >> 8) & 0xff) * FIXED_MUL;
    v.v [2] = ((color >> 16) & 0xff) * FIXED_MUL;

    chafa_vec3i32_sub (&v, &v, &color_table->average);

    v_out [0] = scalar_project_vec3i32 (&v, &color_table->eigenvectors [0], color_table->eigen_mul [0]);
    v_out [1] = scalar_project_vec3i32 (&v, &color_table->eigenvectors [1], color_table->eigen_mul [1]);
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

    vec3i32_fixed_point_from_vec3f32 (&color_table->eigenvectors [0], &eigenvectors [0]);
    vec3i32_fixed_point_from_vec3f32 (&color_table->eigenvectors [1], &eigenvectors [1]);
    vec3i32_fixed_point_from_vec3f32 (&color_table->average, &average);

    color_table->eigen_mul [0] =
        POW2 (color_table->eigenvectors [0].v [0])
        + POW2 (color_table->eigenvectors [0].v [1])
        + POW2 (color_table->eigenvectors [0].v [2]);
    color_table->eigen_mul [0] = MAX (color_table->eigen_mul [0], 1);
    color_table->eigen_mul [0] = FIXED_MUL_BIG / color_table->eigen_mul [0];

    color_table->eigen_mul [1] =
        POW2 (color_table->eigenvectors [1].v [0])
        + POW2 (color_table->eigenvectors [1].v [1])
        + POW2 (color_table->eigenvectors [1].v [2]);
    color_table->eigen_mul [1] = MAX (color_table->eigen_mul [1], 1);
    color_table->eigen_mul [1] = FIXED_MUL_BIG / color_table->eigen_mul [1];

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

    profile_counter_inc (n_a);
    DEBUG_PEN_CHOICE (g_printerr ("a=%d\n", a));

    if (a <= *best_diff)
    {
        b = POW2 (pj->v [1] - v [1]);

        profile_counter_inc (n_b);
        DEBUG_PEN_CHOICE (g_printerr ("b=%d\n", b));

        if (b <= *best_diff)
        {
            d = color_diff (color_table->pens [pj->pen], want_color);

            profile_counter_inc (n_c);
            DEBUG_PEN_CHOICE (g_printerr ("d=%d\n", d));

            if (d <= *best_diff)
            {
                *best_pen = j;
                *best_diff = d;

                profile_counter_inc (n_d);
                DEBUG_PEN_CHOICE (g_printerr ("a=%d, b=%d, d=%d\n", a, b, d));
            }
        }
    }
    else
    {
        DEBUG_PEN_CHOICE (g_printerr ("\n"));
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
#if CHAFA_COLOR_TABLE_ENABLE_PROFILING
    g_printerr ("l=%7d m=%7d a=%7d b=%7d c=%7d d=%7d\n"
                "per probe: a=%6.1lf b=%6.1lf c=%6.1lf d=%6.1lf\n",
                n_lookups,
                n_misses,
                n_a,
                n_b,
                n_c,
                n_d,
                n_a / (gdouble) n_lookups,
                n_b / (gdouble) n_lookups,
                n_c / (gdouble) n_lookups,
                n_d / (gdouble) n_lookups);
#endif
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

    profile_counter_inc (n_lookups);

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

    /* Right scan for closer match */

    for (j = m + 1; j < color_table->n_entries; j++)
    {
        if (!refine_pen_choice (color_table, want_color, v, j, &best_pen, &best_diff))
            break;
    }

#if CHAFA_COLOR_TABLE_ENABLE_PROFILING
    gint best_pen_2 = -1;
    gint best_diff_2 = G_MAXINT;

    for (i = 0; i < color_table->n_entries; i++)
    {
        const ChafaColorTableEntry *pi = &color_table->entries [i];
        gint d = color_diff (color_table->pens [pi->pen], want_color);

        if (d < best_diff_2)
        {
            best_pen_2 = i;
            best_diff_2 = d;
        }
    }

    if (best_diff_2 < best_diff)
    {
        profile_counter_inc (n_misses);

        g_printerr ("Bad lookup: "); dump_entry (color_table, best_pen, want_color);
        g_printerr ("\nShould be:  "); dump_entry (color_table, best_pen_2, want_color);
        g_printerr ("\n");
    }
#endif

    return color_table->entries [best_pen].pen;
}
