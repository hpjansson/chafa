/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2024-2025 Hans Petter Jansson
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

#ifndef __CHAFA_STREAM_WRITER_H__
#define __CHAFA_STREAM_WRITER_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct ChafaStreamWriter ChafaStreamWriter;

ChafaStreamWriter *chafa_stream_writer_new_from_fd (gint fd);
void chafa_stream_writer_ref (ChafaStreamWriter *stream_writer);
void chafa_stream_writer_unref (ChafaStreamWriter *stream_writer);

gint chafa_stream_writer_get_fd (ChafaStreamWriter *stream_writer);
gboolean chafa_stream_writer_is_console (ChafaStreamWriter *stream_writer);

gint chafa_stream_writer_get_buffer_max (ChafaStreamWriter *stream_writer);
void chafa_stream_writer_set_buffer_max (ChafaStreamWriter *stream_writer, gint buf_max);

void chafa_stream_writer_write (ChafaStreamWriter *stream_writer, gconstpointer data, gint len);
gint chafa_stream_writer_print (ChafaStreamWriter *stream_writer, const gchar *format, ...);
gboolean chafa_stream_writer_flush (ChafaStreamWriter *stream_writer);

G_END_DECLS

#endif /* __CHAFA_STREAM_WRITER_H__ */
