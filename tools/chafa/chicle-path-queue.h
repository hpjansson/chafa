/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2025 Hans Petter Jansson
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

#ifndef __CHICLE_PATH_QUEUE_H__
#define __CHICLE_PATH_QUEUE_H__

#include <glib.h>
#include "chafa-stream-reader.h"

G_BEGIN_DECLS

typedef struct ChiclePathQueue ChiclePathQueue;

ChiclePathQueue *chicle_path_queue_new (void);
ChiclePathQueue *chicle_path_queue_ref (ChiclePathQueue *path_queue);
void chicle_path_queue_unref (ChiclePathQueue *path_queue);

void chicle_path_queue_push_path (ChiclePathQueue *path_queue, const gchar *path);
void chicle_path_queue_push_path_list_steal (ChiclePathQueue *path_queue, GList *path_list);
void chicle_path_queue_push_stream (ChiclePathQueue *path_queue, const gchar *stream_path,
                                    const gchar *separator, gint separator_len);
void chicle_path_queue_wait (ChiclePathQueue *path_queue);
gchar *chicle_path_queue_try_pop (ChiclePathQueue *path_queue);
gchar *chicle_path_queue_pop (ChiclePathQueue *path_queue);
gint chicle_path_queue_get_length (ChiclePathQueue *path_queue);
gboolean chicle_path_queue_is_empty (ChiclePathQueue *path_queue);
gint chicle_path_queue_get_n_processed (ChiclePathQueue *path_queue);
gboolean chicle_path_queue_have_stdin_source (ChiclePathQueue *path_queue);

G_END_DECLS

#endif /* __CHICLE_PATH_QUEUE_H__ */
