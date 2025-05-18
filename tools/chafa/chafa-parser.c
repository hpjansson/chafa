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

#include "config.h"

#include <string.h>  /* memcpy, memset */
#include "chafa.h"
#include "chafa-parser.h"

struct ChafaEvent
{
    ChafaEventType type;
    gunichar c;
    ChafaTermSeq seq;
    guint seq_args [CHAFA_TERM_SEQ_ARGS_MAX];
    gint n_seq_args;
};

struct ChafaParser
{
    ChafaTermInfo *term_info;
    GString *buf;
    gint buf_ofs;
    guint eof_pushed : 1;
    guint eof_dispatched : 1;
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

ChafaTermSeq
chafa_event_get_seq (ChafaEvent *event)
{
    g_return_val_if_fail (event != NULL, CHAFA_TERM_SEQ_MAX);

    if (event->type != CHAFA_SEQ_EVENT)
        return CHAFA_TERM_SEQ_MAX;

    return event->seq;
}

gint
chafa_event_get_seq_arg (ChafaEvent *event, gint n)
{
    g_return_val_if_fail (event != NULL, -1);

    if (event->type != CHAFA_SEQ_EVENT
        || n >= event->n_seq_args)
        return -1;

    return (gint) event->seq_args [n];
}

gint
chafa_event_get_n_seq_args (ChafaEvent *event)
{
    g_return_val_if_fail (event != NULL, 0);

    return event->n_seq_args;
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
chafa_parser_destroy (ChafaParser *parser)
{
    g_return_if_fail (parser != NULL);

    chafa_parser_deinit (parser);
    g_free (parser);
}

void
chafa_parser_push_data (ChafaParser *parser, gconstpointer data, gint data_len)
{
    g_return_if_fail (parser != NULL);
    g_return_if_fail (data != NULL);
    g_return_if_fail (data_len >= 0);
    g_return_if_fail (parser->eof_pushed == FALSE);

    g_string_append_len (parser->buf, data, data_len);
}

void
chafa_parser_push_eof (ChafaParser *parser)
{
    g_return_if_fail (parser != NULL);
    g_return_if_fail (parser->eof_pushed == FALSE);

    parser->eof_pushed = TRUE;
}

ChafaEvent *
chafa_parser_pop_event (ChafaParser *parser)
{
    ChafaEvent *event = NULL;
    gboolean have_again = FALSE;
    gchar *p0;
    gint len;
    gint i;

    g_return_val_if_fail (parser != NULL, FALSE);

    p0 = parser->buf->str;
    len = parser->buf->len;

    for (i = 0; i < CHAFA_TERM_SEQ_MAX; i++)
    {
        ChafaParseResult result;
        ChafaEvent temp_event;

        result = chafa_term_info_parse_seq_varargs (parser->term_info, i,
                                                    &p0, &len,
                                                    temp_event.seq_args,
                                                    &temp_event.n_seq_args);
        if (result == CHAFA_PARSE_SUCCESS)
        {
            event = g_memdup (&temp_event, sizeof (ChafaEvent));
            event->type = CHAFA_SEQ_EVENT;
            event->seq = i;
            event->c = 0;
            goto out;
        }
        else if (result == CHAFA_PARSE_AGAIN)
        {
            have_again = TRUE;
        }
    }

    while (!have_again && len > 0)
    {
        gunichar c;

        /* Character */

        c = g_utf8_get_char_validated (p0, len);
        if (c == (gunichar) -1)
        {
            /* Invalid sequence; skip one byte */
            p0++; len--;
        }
        else if (c == (gunichar) -2)
        {
            gchar *p1 = memchr (p0, '\0', len);
            if (p1)
            {
                /* Embedded NUL */
                len -= (ptrdiff_t) p1 - (ptrdiff_t) p0 + 1;
                p0 = p1 + 1;
            }
            else
            {
                /* Incomplete */
                have_again = TRUE;
            }
        }
        else
        {
            /* Good char */
            event = g_new0 (ChafaEvent, 1);
            event->type = CHAFA_UNICHAR_EVENT;
            event->seq = -1;
            event->c = c;
            p0 = g_utf8_next_char (p0);
            goto out;
        }
    }

out:
    if (!event && parser->eof_pushed && !parser->eof_dispatched)
    {
        event = g_new0 (ChafaEvent, 1);
        event->type = CHAFA_EOF_EVENT;
        event->seq = -1;
        parser->eof_dispatched = TRUE;
    }

    if (p0 != parser->buf->str)
    {
        /* FIXME: This will be slow. Switch to a better data structure */
        g_string_erase (parser->buf, 0, (ptrdiff_t) p0 - (ptrdiff_t) parser->buf->str);
    }

    return event;
}
