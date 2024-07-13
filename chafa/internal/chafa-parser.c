/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2022 Hans Petter Jansson
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

#include "config.h"

#include <string.h>  /* memcpy, memset */
#include "chafa.h"
#include "internal/chafa-private.h"
#include "internal/chafa-parser.h"

#define DEBUG(x)

/* --- *
 * API *
 * --- */

typedef enum
{
    CHAFA_EOF_EVENT,
    CHAFA_UNICHAR_EVENT,
    CHAFA_SEQ_EVENT,
}
ChafaEventType;

struct ChafaEvent
{
    ChafaEventType type;
    gunichar c;
    ChafaTermSeq seq;
    guint seq_args [CHAFA_TERM_SEQ_ARGS_MAX];
};

struct ChafaParser
{
    ChafaTermInfo *term_info;
    GString *buf;
    gint buf_ofs;
};

ChafaEventType
chafa_event_get_type (ChafaEvent *event)
{
    g_return_val_if_fail (event != NULL, CHAFA_EOF_EVENT);

    return event->type;
}

gunichar
chafa_event_get_unichar (ChafaEvent *event)
{
    g_return_val_if_fail (event != NULL, 0);
    g_return_val_if_fail (event->type == CHAFA_UNICHAR_EVENT, 0);

    return event->c;
}

void
chafa_parser_init (ChafaParser *parser_out, ChafaTermInfo *term_info)
{
    g_return_if_fail (parser_out != NULL);

    parser_out->term_info = term_info;
    chafa_term_info_ref (term_info);
    parser_out->buf = g_string_new ("");
}

void
chafa_parser_deinit (ChafaParser *parser)
{
    g_return_if_fail (parser != NULL);

    chafa_term_info_unref (parser->term_info);
    g_string_free (parser->buf, TRUE);
}

ChafaParser *
chafa_parser_new (ChafaTermInfo *term_info)
{
    ChafaParser *parser;

    parser = g_new0 (ChafaParser, 1);
    chafa_parser_init (parser, term_info);
    return parser;
}

void
chafa_parser_push (ChafaParser *parser, const gchar *input, gint input_len)
{
    g_return_if_fail (parser != NULL);
    g_return_if_fail (input != NULL);
    g_return_if_fail (input_len >= 0);

    g_string_append_len (parser->buf, input, input_len);
}

gboolean
chafa_parser_pop (ChafaParser *parser, ChafaEvent *event_out)
{
    g_return_val_if_fail (parser != NULL, FALSE);
    g_return_val_if_fail (event_out != NULL, FALSE);

}
