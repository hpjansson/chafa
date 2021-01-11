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

#ifndef __FILE_MAPPING_H__
#define __FILE_MAPPING_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct FileMapping FileMapping;

FileMapping *file_mapping_new (const gchar *path);
void file_mapping_destroy (FileMapping *file_mapping);

gboolean file_mapping_taste (FileMapping *file_mapping, gpointer out, goffset ofs, gsize length);
gconstpointer file_mapping_get_data (FileMapping *file_mapping, gsize *length_out);

gboolean file_mapping_has_magic (FileMapping *file_mapping, goffset ofs,
                                 gconstpointer data, gsize length);

G_END_DECLS

#endif /* __FILE_MAPPING_H__ */
