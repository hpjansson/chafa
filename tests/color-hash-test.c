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
#include "internal/chafa-color-hash.h"

/* The fill in chafa_color_hash_init() relies on color i + 1 never hashing
 * to bucket i. Check every bucket, so a change to the hash function that
 * breaks the assumption is caught here. */
static void
sentinel_test (void)
{
    gint i;

    for (i = 0; i < CHAFA_COLOR_HASH_N_BUCKETS; i++)
        g_assert_cmpuint (_chafa_color_hash_calc_bucket ((guint32) (i + 1)), !=, (guint) i);
}

/* A fresh table must miss on every color, including white and black */
static void
empty_test (void)
{
    ChafaColorHash hash;
    guint32 color;

    chafa_color_hash_init (&hash);

    g_assert_cmpint (chafa_color_hash_lookup (&hash, 0x000000), ==, -1);
    g_assert_cmpint (chafa_color_hash_lookup (&hash, 0xffffff), ==, -1);

    /* Check all 2^24 colors. We can afford it. */
    for (color = 0; color < 0x1000000; color += 1)
        g_assert_cmpint (chafa_color_hash_lookup (&hash, color), ==, -1);

    chafa_color_hash_deinit (&hash);
}

/* Replace, hit, and LRU replacement within a bucket */
static void
lru_test (void)
{
    ChafaColorHash hash;
    guint32 colors [CHAFA_COLOR_HASH_N_WAYS + 1];
    guint bucket;
    gint n, i;

    chafa_color_hash_init (&hash);

    /* Find N_WAYS + 1 distinct colors sharing a bucket */

    colors [0] = 0x123456;
    bucket = _chafa_color_hash_calc_bucket (colors [0]);

    for (n = 1, i = 1; n < CHAFA_COLOR_HASH_N_WAYS + 1; i++)
    {
        guint32 color = (colors [0] + (guint32) i * 65537) & 0xffffff;

        if (color != colors [0] && _chafa_color_hash_calc_bucket (color) == bucket)
            colors [n++] = color;
    }

    /* White is an ordinary key */

    chafa_color_hash_replace (&hash, 0xffffff, 7);
    g_assert_cmpint (chafa_color_hash_lookup (&hash, 0xffffff), ==, 7);

    /* Fill the bucket; all N_WAYS entries hit, with their own pens */

    for (i = 0; i < CHAFA_COLOR_HASH_N_WAYS; i++)
        chafa_color_hash_replace (&hash, colors [i], i);
    for (i = 0; i < CHAFA_COLOR_HASH_N_WAYS; i++)
        g_assert_cmpint (chafa_color_hash_lookup (&hash, colors [i]), ==, i);

    /* Touch the oldest entry, then insert one more: the second oldest goes */

    g_assert_cmpint (chafa_color_hash_lookup (&hash, colors [0]), ==, 0);
    chafa_color_hash_replace (&hash, colors [CHAFA_COLOR_HASH_N_WAYS], 99);
    g_assert_cmpint (chafa_color_hash_lookup (&hash, colors [0]), ==, 0);
    g_assert_cmpint (chafa_color_hash_lookup (&hash, colors [1]), ==, -1);
    g_assert_cmpint (chafa_color_hash_lookup (&hash, colors [CHAFA_COLOR_HASH_N_WAYS]), ==, 99);

    /* Alpha is not part of the key */

    g_assert_cmpint (chafa_color_hash_lookup (&hash, colors [0] | 0xff000000), ==, 0);

    chafa_color_hash_deinit (&hash);
}

int
main (int argc, char *argv [])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/color-hash/sentinel", sentinel_test);
    g_test_add_func ("/color-hash/empty", empty_test);
    g_test_add_func ("/color-hash/lru", lru_test);

    return g_test_run ();
}
