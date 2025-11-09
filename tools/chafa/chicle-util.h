/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2025 Hans Petter Jansson
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

#ifndef __CHICLE_UTIL_H__
#define __CHICLE_UTIL_H__

#include <glib.h>
#include "chafa-term.h"

G_BEGIN_DECLS

typedef enum
{
    CHICLE_ROTATION_NONE = 0,
    CHICLE_ROTATION_0 = 1,
    CHICLE_ROTATION_0_MIRROR = 2,
    CHICLE_ROTATION_180 = 3,
    CHICLE_ROTATION_180_MIRROR = 4,
    CHICLE_ROTATION_270_MIRROR = 5,
    CHICLE_ROTATION_270 = 6,
    CHICLE_ROTATION_90_MIRROR = 7,
    CHICLE_ROTATION_90 = 8,
    CHICLE_ROTATION_UNDEFINED = 9,

    CHICLE_ROTATION_MAX
}
ChicleRotationType;

ChicleRotationType chicle_invert_rotation (ChicleRotationType rot);
void chicle_rotate_image (gpointer *src, guint *width, guint *height, guint *rowstride,
                          guint n_channels, ChicleRotationType rot);

void chicle_flatten_cntrl_inplace (gchar *str);
gchar *chicle_ellipsize_string (const gchar *str, gint len_max,
                                gboolean use_unicode);
gchar *chicle_path_get_ellipsized_basename (const gchar *path, gint len_max,
                                            gboolean use_unicode);
void chicle_print_rep_char (ChafaTerm *term, gchar c, gint n);
void chicle_path_print_label (ChafaTerm *term, const gchar *path, ChafaAlign halign,
                              gint field_width, gboolean use_unicode, gboolean link_label);

G_END_DECLS

#endif /* __CHICLE_UTIL_H__ */
