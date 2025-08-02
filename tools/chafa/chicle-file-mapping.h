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

#ifndef __CHICLE_FILE_MAPPING_H__
#define __CHICLE_FILE_MAPPING_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct ChicleFileMapping ChicleFileMapping;

ChicleFileMapping *chicle_file_mapping_new (const gchar *path);
void chicle_file_mapping_destroy (ChicleFileMapping *file_mapping);

gboolean chicle_file_mapping_open_now (ChicleFileMapping *file_mapping, GError **error);

const gchar *chicle_file_mapping_get_path (ChicleFileMapping *file_mapping);

gboolean chicle_file_mapping_taste (ChicleFileMapping *file_mapping, gpointer out, goffset ofs, gsize length);
gssize chicle_file_mapping_read (ChicleFileMapping *file_mapping, gpointer out, goffset ofs, gsize length);
gconstpointer chicle_file_mapping_get_data (ChicleFileMapping *file_mapping, gsize *length_out);

gboolean chicle_file_mapping_has_magic (ChicleFileMapping *file_mapping, goffset ofs,
                                        gconstpointer data, gsize length);

G_END_DECLS

#endif /* __CHICLE_FILE_MAPPING_H__ */
