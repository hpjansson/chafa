/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2024 Hans Petter Jansson
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

#ifndef __CHAFA_PARSER_H__
#define __CHAFA_PARSER_H__

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
    CHAFA_EOF_EVENT,
    CHAFA_UNICHAR_EVENT,
    CHAFA_SEQ_EVENT,
}
ChafaEventType;

typedef struct ChafaEvent ChafaEvent;
typedef struct ChafaParser ChafaParser;

ChafaEventType chafa_event_get_type (ChafaEvent *event);
gunichar chafa_event_get_unichar (ChafaEvent *event);
ChafaTermSeq chafa_event_get_seq (ChafaEvent *event);
gint chafa_event_get_seq_arg (ChafaEvent *event, gint n);
gint chafa_event_get_n_seq_args (ChafaEvent *event);

ChafaParser *chafa_parser_new (ChafaTermInfo *term_info);
void chafa_parser_destroy (ChafaParser *parser);
void chafa_parser_init (ChafaParser *parser_out, ChafaTermInfo *term_info);
void chafa_parser_deinit (ChafaParser *parser);

void chafa_parser_push_data (ChafaParser *parser, gconstpointer data, gint data_len);
void chafa_parser_push_eof (ChafaParser *parser);
ChafaEvent *chafa_parser_pop_event (ChafaParser *parser);

G_END_DECLS

#endif /* __CHAFA_PARSER_H__ */
