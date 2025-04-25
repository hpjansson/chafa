/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2021-2024 Hans Petter Jansson
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

#ifndef __CHAFA_MATH_UTIL_H__
#define __CHAFA_MATH_UTIL_H__

#include <glib.h>

G_BEGIN_DECLS

#define CHAFA_SQUARE(n) ((n) * (n))

void chafa_tuck_and_align (gint src_width, gint src_height,
                           gint dest_width, gint dest_height,
                           ChafaAlign halign, ChafaAlign valign,
                           ChafaTuck tuck,
                           gint *ofs_x_out, gint *ofs_y_out,
                           gint *width_out, gint *height_out);

gint round_up_to_multiple_of (gint value, gint m);

G_END_DECLS

#endif /* __CHAFA_MATH_UTIL_H__ */
