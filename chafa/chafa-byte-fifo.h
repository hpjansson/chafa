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

#ifndef __CHAFA_BYTE_FIFO_H__
#define __CHAFA_BYTE_FIFO_H__

#if !defined (__CHAFA_H_INSIDE__) && !defined (CHAFA_COMPILATION)
# error "Only <chafa.h> can be included directly."
#endif

#include <glib.h>

G_BEGIN_DECLS

typedef struct ChafaByteFifo ChafaByteFifo;

CHAFA_AVAILABLE_IN_1_20
ChafaByteFifo *chafa_byte_fifo_new (void);
CHAFA_AVAILABLE_IN_1_20
ChafaByteFifo *chafa_byte_fifo_ref (ChafaByteFifo *byte_fifo);
CHAFA_AVAILABLE_IN_1_20
void chafa_byte_fifo_unref (ChafaByteFifo *byte_fifo);

CHAFA_AVAILABLE_IN_1_20
gint64 chafa_byte_fifo_get_pos (ChafaByteFifo *byte_fifo);
CHAFA_AVAILABLE_IN_1_20
gint chafa_byte_fifo_get_len (ChafaByteFifo *byte_fifo);
CHAFA_AVAILABLE_IN_1_20
void chafa_byte_fifo_push (ChafaByteFifo *byte_fifo, gconstpointer src, gint src_len);
CHAFA_AVAILABLE_IN_1_20
gint chafa_byte_fifo_pop (ChafaByteFifo *byte_fifo, gpointer dest, gint dest_len);
CHAFA_AVAILABLE_IN_1_20
gconstpointer chafa_byte_fifo_peek (ChafaByteFifo *byte_fifo, gint *len);
CHAFA_AVAILABLE_IN_1_20
gint chafa_byte_fifo_drop (ChafaByteFifo *byte_fifo, gint len);
CHAFA_AVAILABLE_IN_1_20
gint chafa_byte_fifo_search (ChafaByteFifo *byte_fifo,
                             gconstpointer data, gint data_len,
                             gint64 *pos);
CHAFA_AVAILABLE_IN_1_20
gpointer chafa_byte_fifo_split_next (ChafaByteFifo *byte_fifo,
                                     gconstpointer separator, gint separator_len,
                                     gint64 *restart_pos, gint *len_out);

G_END_DECLS

#endif /* __CHAFA_BYTE_FIFO_H__ */
