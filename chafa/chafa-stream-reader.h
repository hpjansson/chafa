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

#ifndef __CHAFA_STREAM_READER_H__
#define __CHAFA_STREAM_READER_H__

#if !defined (__CHAFA_H_INSIDE__) && !defined (CHAFA_COMPILATION)
# error "Only <chafa.h> can be included directly."
#endif

#include <glib.h>

G_BEGIN_DECLS

typedef struct ChafaStreamReader ChafaStreamReader;

CHAFA_AVAILABLE_IN_1_20
ChafaStreamReader *chafa_stream_reader_new_from_fd (gint fd);
CHAFA_AVAILABLE_IN_1_20
ChafaStreamReader *chafa_stream_reader_new_from_fd_full (gint fd,
                                                         gconstpointer token_separator,
                                                         gint token_separator_len);
CHAFA_AVAILABLE_IN_1_20
void chafa_stream_reader_ref (ChafaStreamReader *stream_reader);
CHAFA_AVAILABLE_IN_1_20
void chafa_stream_reader_unref (ChafaStreamReader *stream_reader);

CHAFA_AVAILABLE_IN_1_20
gint chafa_stream_reader_get_fd (ChafaStreamReader *stream_reader);
CHAFA_AVAILABLE_IN_1_20
gboolean chafa_stream_reader_is_console (ChafaStreamReader *stream_reader);

CHAFA_AVAILABLE_IN_1_20
gint chafa_stream_reader_read (ChafaStreamReader *stream_reader, gpointer out, gint max_len);
CHAFA_AVAILABLE_IN_1_20
gint chafa_stream_reader_read_token (ChafaStreamReader *stream_reader, gpointer *out, gint max_len);

CHAFA_AVAILABLE_IN_1_20
gboolean chafa_stream_reader_wait_until (ChafaStreamReader *stream_reader, gint64 end_time_us);
CHAFA_AVAILABLE_IN_1_20
void chafa_stream_reader_wait (ChafaStreamReader *stream_reader, gint timeout_ms);

CHAFA_AVAILABLE_IN_1_20
gboolean chafa_stream_reader_is_eof (ChafaStreamReader *stream_reader);

G_END_DECLS

#endif /* __CHAFA_STREAM_READER_H__ */
