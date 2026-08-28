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

#ifndef __CHAFA_COLOR_HASH_H__
#define __CHAFA_COLOR_HASH_H__

G_BEGIN_DECLS

/* A small set-associative cache mapping 24-bit colors to 8-bit pens. Each
 * bucket holds a few entries ordered from most to least recently used. A
 * hit moves its entry to the front, and an insertion evicts the last one.
 * Entries are stored as (color << 8) | pen. There is no reserved empty
 * value; unused entries hold a color that hashes to a different bucket, so
 * they can never match a lookup. */

#define CHAFA_COLOR_HASH_N_WAYS_SHIFT 2
#define CHAFA_COLOR_HASH_N_WAYS (1 << CHAFA_COLOR_HASH_N_WAYS_SHIFT)
#define CHAFA_COLOR_HASH_N_BUCKETS_SHIFT 14
#define CHAFA_COLOR_HASH_N_BUCKETS (1 << CHAFA_COLOR_HASH_N_BUCKETS_SHIFT)
#define CHAFA_COLOR_HASH_N_ENTRIES (CHAFA_COLOR_HASH_N_BUCKETS * CHAFA_COLOR_HASH_N_WAYS)

typedef struct
{
    guint32 *map;
}
ChafaColorHash;

void   chafa_color_hash_init    (ChafaColorHash *color_hash);
void   chafa_color_hash_deinit  (ChafaColorHash *color_hash);

static inline guint
_chafa_color_hash_calc_bucket (guint32 color)
{
    guint32 c = color & 0x00ffffffU;

    /* Measured with a good distribution. The carry spills across channels. If
     * this ever changes, we must also revise the table init. */
    return (c + (c >> 12)) & (CHAFA_COLOR_HASH_N_BUCKETS - 1);
}

static inline guint32 *
_chafa_color_hash_get_bucket (const ChafaColorHash *color_hash, guint32 color)
{
    return color_hash->map + (_chafa_color_hash_calc_bucket (color) << CHAFA_COLOR_HASH_N_WAYS_SHIFT);
}

static inline void
chafa_color_hash_replace (ChafaColorHash *color_hash, guint32 color, guint8 pen)
{
    guint32 *bucket = _chafa_color_hash_get_bucket (color_hash, color);
    guint32 entry = ((color & 0x00ffffffU) << 8) | pen;

    bucket [3] = bucket [2];
    bucket [2] = bucket [1];
    bucket [1] = bucket [0];
    bucket [0] = entry;
}

static inline gint
chafa_color_hash_lookup (ChafaColorHash *color_hash, guint32 color)
{
    guint32 *bucket = _chafa_color_hash_get_bucket (color_hash, color);
    guint32 want = (color & 0x00ffffffU) << 8;
    guint32 entry;

    entry = bucket [0];
    if ((entry & 0xffffff00U) == want)
        return entry & 0xff;

    entry = bucket [1];
    if ((entry & 0xffffff00U) == want)
    {
        bucket [1] = bucket [0];
        bucket [0] = entry;
        return entry & 0xff;
    }

    entry = bucket [2];
    if ((entry & 0xffffff00U) == want)
    {
        bucket [2] = bucket [1];
        bucket [1] = bucket [0];
        bucket [0] = entry;
        return entry & 0xff;
    }

    entry = bucket [3];
    if ((entry & 0xffffff00U) == want)
    {
        bucket [3] = bucket [2];
        bucket [2] = bucket [1];
        bucket [1] = bucket [0];
        bucket [0] = entry;
        return entry & 0xff;
    }

    return -1;
}

G_END_DECLS

#endif /* __CHAFA_COLOR_HASH_H__ */
