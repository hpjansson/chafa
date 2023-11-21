/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2023 Hans Petter Jansson
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

/* Authors: oshaboy@github */

#ifndef CHARACTER_CANVAS_TO_CONHOST_H
#define CHARACTER_CANVAS_TO_CONHOST_H

#include <glib.h>
#include <chafa.h>
#include <windows.h>

typedef WORD ConhostAttribute;
typedef struct
{
    gunichar2 *str;
    ConhostAttribute *attributes;
    size_t length;
    size_t utf16_string_length;
}
ConhostRow;

/* We must determine if stdout is redirected to a file, and if so, use a
 * different set of I/O functions. */
extern gboolean win32_stdout_is_file;

gboolean safe_WriteConsoleA (HANDLE chd, const gchar *data, gsize len);
gboolean safe_WriteConsoleW (HANDLE chd, const gunichar2 *data, gsize len);
gssize canvas_to_conhost (ChafaCanvas *canvas, ConhostRow **lines);
void write_image_conhost (const ConhostRow *lines, gsize s);
void destroy_lines (ConhostRow *lines, gsize s);

#endif
