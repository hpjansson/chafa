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

#ifndef __CHAFA_DITHER_H__
#define __CHAFA_DITHER_H__

#include "internal/chafa-palette.h"

G_BEGIN_DECLS

typedef struct
{
    ChafaDitherMode mode;
    gdouble intensity;
    gint grain_width_shift;
    gint grain_height_shift;

    gint bayer_size_shift;
    guint bayer_size_mask;
    gint *bayer_matrix;
}
ChafaDither;

void chafa_dither_init (ChafaDither *dither, ChafaDitherMode mode,
                        gdouble intensity,
                        gint grain_width, gint grain_height);
void chafa_dither_deinit (ChafaDither *dither);
void chafa_dither_copy (const ChafaDither *src, ChafaDither *dest);

ChafaColor chafa_dither_color_ordered (const ChafaDither *dither, ChafaColor color, gint x, gint y);

G_END_DECLS

#endif /* __CHAFA_DITHER_H__ */
