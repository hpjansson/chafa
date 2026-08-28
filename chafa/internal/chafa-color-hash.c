/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2019-2025 Hans Petter Jansson
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

#include "chafa.h"
#include "internal/chafa-color-hash.h"

void
chafa_color_hash_init (ChafaColorHash *color_hash)
{
    guint32 *bucket;
    gint i, j;

    color_hash->map = g_malloc (CHAFA_COLOR_HASH_N_ENTRIES * sizeof (guint32));

    /* Fill with invalid entries. Compilers can reduce this to SIMD, in which
     * case it's almost as fast as memset(). */
    for (i = 0, bucket = color_hash->map;
         i < CHAFA_COLOR_HASH_N_BUCKETS;
         i++, bucket += CHAFA_COLOR_HASH_N_WAYS)
    {
        guint32 entry = (guint32) (i + 1) << 8;

        for (j = 0; j < CHAFA_COLOR_HASH_N_WAYS; j++)
            bucket [j] = entry;
    }
}

void
chafa_color_hash_deinit (ChafaColorHash *color_hash)
{
    g_free (color_hash->map);
    color_hash->map = NULL;
}
