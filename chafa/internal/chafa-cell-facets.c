/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2023 Hans Petter Jansson
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

#include <math.h>
#include <string.h>
#include <glib.h>
#include "chafa.h"
#include "internal/chafa-private.h"
#include "internal/chafa-pixops.h"
#include "internal/chafa-cell-facets.h"

/* ==================================== *
 * Alignment-insensitive shape analysis *
 * ==================================== */

/* Lower right quadrant of facet kernel */
static const guint8 quadrant_sectors [CHAFA_SYMBOL_N_PIXELS] =
{
    0,1,1,4,4,4,7,7,
    3,2,2,4,4,4,7,7,
    3,2,5,5,4,7,7,7,
    6,6,5,5,5,8,7,7,
    6,6,6,5,8,8,8,7,
    6,6,9,8,8,8,8,8,
    9,9,9,9,8,8,8,8,
    9,9,9,9,9,8,8,8
};

/* Weight multipliers. A fully populated sector times its weight should
 * equal every other fully populated sector multiplied by its weight.
 *
 * The final value of a weighted facet is in the range [0..1680] inclusive. */
static const guint16 sector_weights [CHAFA_CELL_FACETS_N_SECTORS_PER_QUADRANT] =
{
    /* Normalizing weights */
    2 * 2 * 2 * 2 * 5 * 3 * 7, /* (* 1)             */
    2 * 2 * 2 * 5 * 3 * 7,     /* (* 2)             */
    2 * 2 * 2 * 2 * 5 * 7,     /* (* 3)             */
    2 * 2 * 2 * 5 * 3 * 7,     /* (* 2)             */
    2 * 2 * 2 * 2 * 5 * 3,     /* (* 7)             */
    2 * 2 * 2 * 5 * 7,         /* (* 2 * 3)         */
    2 * 2 * 2 * 2 * 5 * 3,     /* (* 7)             */
    2 * 2 * 2 * 3 * 7,         /* (* 2 * 5)         */
    5 * 3 * 7,                 /* (* 2 * 2 * 2 * 2) */
    2 * 2 * 2 * 3 * 7,         /* (* 2 * 5)         */
};

static guint64 facet_bitmaps [CHAFA_CELL_FACETS_N_FACETS];

static guint64
sector_to_bitmap (const guint8 *block, guint8 sector_index)
{
    guint64 bitmap = 0;
    gint i;

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        bitmap <<= 1;
        if (block [i] == sector_index)
            bitmap |= 1;
    }

    return bitmap;
}

static void
gen_sector_pattern (guint8 *sectors_out, gint x_ofs, gint y_ofs)
{
    gint x, y;

    g_assert (x_ofs >= 0 && x_ofs < (CHAFA_SYMBOL_WIDTH_PIXELS + 1));
    g_assert (y_ofs >= 0 && y_ofs < (CHAFA_SYMBOL_HEIGHT_PIXELS + 1));

    for (y = 0; y < CHAFA_SYMBOL_HEIGHT_PIXELS; y++)
    {
        for (x = 0; x < CHAFA_SYMBOL_WIDTH_PIXELS; x++)
        {
            gint x_index = x - x_ofs;
            gint y_index = y - y_ofs;
            guint8 quadrant_base = 0;

            if (x_index < 0)
            {
                x_index = -x_index - 1;
                quadrant_base += CHAFA_CELL_FACETS_N_SECTORS_PER_QUADRANT;
            }

            if (y_index < 0)
            {
                y_index = -y_index - 1;
                quadrant_base += CHAFA_CELL_FACETS_N_SECTORS_PER_QUADRANT * 2;
            }

            g_assert (x_index >= 0 && x_index < CHAFA_SYMBOL_WIDTH_PIXELS);
            g_assert (y_index >= 0 && y_index < CHAFA_SYMBOL_HEIGHT_PIXELS);

            sectors_out [y * CHAFA_SYMBOL_WIDTH_PIXELS + x]
                = quadrant_base + quadrant_sectors [y_index * CHAFA_SYMBOL_WIDTH_PIXELS + x_index];
        }
    }
}

static void
gen_facet_bitmaps (guint64 *bitmaps_out)
{
    gint i, j;

    for (i = 0; i < CHAFA_CELL_FACETS_N_SAMPLES; i++)
    {
        guint8 sample_pattern [CHAFA_SYMBOL_N_PIXELS];

        gen_sector_pattern (sample_pattern,
                            i % (CHAFA_SYMBOL_WIDTH_PIXELS + 1),
                            i / (CHAFA_SYMBOL_HEIGHT_PIXELS + 1));

        for (j = 0; j < CHAFA_CELL_FACETS_N_SECTORS; j++)
        {
            *(bitmaps_out++) = sector_to_bitmap (sample_pattern, j);
        }
    }
}

static gpointer
init_facet_bitmaps_once (G_GNUC_UNUSED gpointer data)
{
    gen_facet_bitmaps (facet_bitmaps);
    return NULL;
}

static void
init_facet_bitmaps (void)
{
    static GOnce once = G_ONCE_INIT;
    g_once (&once, init_facet_bitmaps_once, NULL);
}

static void
calc_facets (ChafaCellFacets *facets_out, guint64 bitmap)
{
    gint fmap [CHAFA_CELL_FACETS_N_FACETS];
    gint i, j;

    init_facet_bitmaps ();

    chafa_intersection_count_vu64 (bitmap, facet_bitmaps, fmap, CHAFA_CELL_FACETS_N_FACETS);

    for (i = 0; i < CHAFA_CELL_FACETS_N_SAMPLES; i++)
    {
        for (j = 0; j < CHAFA_CELL_FACETS_N_SECTORS; j++)
        {
            facets_out->facets [i * CHAFA_CELL_FACETS_N_SECTORS + j]
                = (fmap [i * CHAFA_CELL_FACETS_N_SECTORS + j]
                   * sector_weights [j % CHAFA_CELL_FACETS_N_SECTORS_PER_QUADRANT]);
        }
    }
}

void
chafa_cell_facets_from_bitmap (ChafaCellFacets *cell_facets_out, guint64 bitmap)
{
    calc_facets (cell_facets_out, bitmap);
}

/* With 3240 facets and the max facet value at 1680, the error will be in the
 * range [0..9144576000] inclusive, requiring 34 bits of storage. */
gint64
chafa_cell_facets_distance (const ChafaCellFacets *a, const ChafaCellFacets *b)
{
    gint64 badness = 0;
    gint i;

    for (i = 0; i < CHAFA_CELL_FACETS_N_FACETS; i++)
    {
        gint e = (gint) a->facets [i] - (gint) b->facets [i];
#if 0
        badness += e * e;
#else
        badness += abs (e);
#endif

#if 0
        g_printerr ("a=%d b=%d\n", a->facets [i], b->facets [i]);
#endif
    }

#if 1
    return badness;
#elif 0
    return (badness >> 6);  /* 28 bits */
#else
    return badness * badness;
#endif
}
