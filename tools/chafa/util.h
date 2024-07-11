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

#ifndef __UTIL_H__
#define __UTIL_H__

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
    ROTATION_NONE = 0,
    ROTATION_0 = 1,
    ROTATION_0_MIRROR = 2,
    ROTATION_180 = 3,
    ROTATION_180_MIRROR = 4,
    ROTATION_270_MIRROR = 5,
    ROTATION_270 = 6,
    ROTATION_90_MIRROR = 7,
    ROTATION_90 = 8,
    ROTATION_UNDEFINED = 9,

    ROTATION_MAX
}
RotationType;

RotationType invert_rotation (RotationType rot);
void rotate_image (gpointer *src, guint *width, guint *height, guint *rowstride,
                   guint n_channels, RotationType rot);

G_END_DECLS

#endif /* __UTIL_H__ */
