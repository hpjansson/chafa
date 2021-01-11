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

#ifndef __CHAFA_COLOR_TABLE_H__
#define __CHAFA_COLOR_TABLE_H__

#include "internal/chafa-pca.h"

G_BEGIN_DECLS

#define CHAFA_COLOR_TABLE_MAX_ENTRIES 256

typedef struct
{
    gint v [2];
    gint pen;
}
ChafaColorTableEntry;

typedef struct
{
    ChafaColorTableEntry entries [CHAFA_COLOR_TABLE_MAX_ENTRIES];

    /* Each pen is 24 bits (B8G8R8) of color information */
    guint32 pens [CHAFA_COLOR_TABLE_MAX_ENTRIES];

    gint n_entries;
    guint is_sorted : 1;

    ChafaVec3i32 eigenvectors [2];
    ChafaVec3i32 average;

    guint eigen_mul [2];
}
ChafaColorTable;

void       chafa_color_table_init             (ChafaColorTable *color_table);
void       chafa_color_table_deinit           (ChafaColorTable *color_table);

guint32    chafa_color_table_get_pen_color    (const ChafaColorTable *color_table, gint pen);
void       chafa_color_table_set_pen_color    (ChafaColorTable *color_table, gint pen, guint32 color);

void       chafa_color_table_sort             (ChafaColorTable *color_table);
gint       chafa_color_table_find_nearest_pen (const ChafaColorTable *color_table, guint32 color);

G_END_DECLS

#endif /* __CHAFA_COLOR_TABLE_H__ */
