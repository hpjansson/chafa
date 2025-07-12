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

#ifndef __PATH_QUEUE_H__
#define __PATH_QUEUE_H__

#include <glib.h>
#include "chafa-stream-reader.h"

G_BEGIN_DECLS

typedef struct PathQueue PathQueue;

PathQueue *path_queue_new (void);
PathQueue *path_queue_ref (PathQueue *path_queue);
void path_queue_unref (PathQueue *path_queue);

void path_queue_push_path (PathQueue *path_queue, const gchar *path);
void path_queue_push_path_list_steal (PathQueue *path_queue, GList *path_list);
void path_queue_push_stream (PathQueue *path_queue, const gchar *stream_path,
                            const gchar *separator, gint separator_len);
void path_queue_wait (PathQueue *path_queue);
gchar *path_queue_try_pop (PathQueue *path_queue);
gchar *path_queue_pop (PathQueue *path_queue);
gint path_queue_get_length (PathQueue *path_queue);
gboolean path_queue_is_empty (PathQueue *path_queue);
gint path_queue_get_n_processed (PathQueue *path_queue);

G_END_DECLS

#endif /* __PATH_QUEUE_H__ */
