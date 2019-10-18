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
#include "internal/chafa-color-hash.h"

static guint
calc_hash (guint32 color)
{
    color &= 0x00ffffff;

    return (color ^ (color >> 7) ^ (color >> 14)) % CHAFA_COLOR_HASH_N_ENTRIES;
}

void
chafa_color_hash_init (ChafaColorHash *color_hash)
{
    memset (color_hash->map, 0, sizeof (color_hash->map));
}

void
chafa_color_hash_deinit (G_GNUC_UNUSED ChafaColorHash *color_hash)
{
}

void
chafa_color_hash_replace (ChafaColorHash *color_hash, guint32 color, guint8 pen)
{
    guint index = calc_hash (color);
    guint32 entry = (color << 8) | pen;

    color_hash->map [index] = entry;
}

guint8
chafa_color_hash_lookup (const ChafaColorHash *color_hash, guint32 color)
{
    guint index = calc_hash (color);
    guint32 entry = color_hash->map [index];

    if ((entry & 0xffffff00) == (color << 8))
        return entry & 0xff;

    return 0;
}
