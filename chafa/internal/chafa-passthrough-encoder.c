/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2023 Hans Petter Jansson
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

#include "chafa.h"
#include "internal/chafa-passthrough-encoder.h"

#define ESCAPE_BUF_SIZE 1024

static const gint packet_size_max [CHAFA_PASSTHROUGH_MAX] =
{
    /* Screen's OSC buffer size was increased to 2560 in bfb05c34ba1f961a15ccea04c5.
     * This was quite a while ago, but it appears it still hasn't made its way into
     * some of the important OS distributions. */
    [CHAFA_PASSTHROUGH_SCREEN] = 200,
    [CHAFA_PASSTHROUGH_TMUX] = 1000000
};

static void
append_begin (ChafaPassthroughEncoder *ptenc)
{
    gchar seq [CHAFA_TERM_SEQ_LENGTH_MAX + 1];

    if (ptenc->mode == CHAFA_PASSTHROUGH_SCREEN)
    {
        *chafa_term_info_emit_begin_screen_passthrough (ptenc->term_info, seq) = '\0';
        g_string_append (ptenc->out, seq);
    }
    else if (ptenc->mode == CHAFA_PASSTHROUGH_TMUX)
    {
        *chafa_term_info_emit_begin_tmux_passthrough (ptenc->term_info, seq) = '\0';
        g_string_append (ptenc->out, seq);
    }
}

static void
append_end (ChafaPassthroughEncoder *ptenc)
{
    gchar seq [CHAFA_TERM_SEQ_LENGTH_MAX + 1];

    if (ptenc->mode == CHAFA_PASSTHROUGH_SCREEN)
    {
        *chafa_term_info_emit_end_screen_passthrough (ptenc->term_info, seq) = '\0';
        g_string_append (ptenc->out, seq);
    }
    else if (ptenc->mode == CHAFA_PASSTHROUGH_TMUX)
    {
        *chafa_term_info_emit_end_tmux_passthrough (ptenc->term_info, seq) = '\0';
        g_string_append (ptenc->out, seq);
    }
}

static void
append_packetized (ChafaPassthroughEncoder *ptenc, const gchar *in, gint len)
{
    while (len > 0)
    {
        gssize remain = packet_size_max [ptenc->mode] - ptenc->packet_size;
        gint n;

        if (remain == 0)
        {
            append_end (ptenc);
            ptenc->packet_size = 0;
            remain = packet_size_max [ptenc->mode];
        }

        if (ptenc->packet_size == 0)
        {
            append_begin (ptenc);
        }

        n = MIN (len, remain);

        g_string_append_len (ptenc->out, in, n);

        len -= n;
        ptenc->packet_size += n;
        in += n;
    }
}

static void
append_escaped (ChafaPassthroughEncoder *ptenc, const gchar *in, gint len)
{
    gchar buf [ESCAPE_BUF_SIZE];
    gint i, j;

    for (i = 0, j = 0; i < len; i++)
    {
        buf [j++] = in [i];
        if (in [i] == 0x1b)
            buf [j++] = 0x1b;

        if (j + 2 > ESCAPE_BUF_SIZE)
        {
            append_packetized (ptenc, buf, j);
            j = 0;
        }
    }

    append_packetized (ptenc, buf, j);
}

void
chafa_passthrough_encoder_begin (ChafaPassthroughEncoder *ptenc,
                                 ChafaPassthrough passthrough,
                                 ChafaTermInfo *term_info, GString *out_str)
{
    ptenc->mode = passthrough;
    ptenc->term_info = term_info;
    chafa_term_info_ref (term_info);
    ptenc->out = out_str;
    ptenc->packet_size = 0;
}

void
chafa_passthrough_encoder_end (ChafaPassthroughEncoder *ptenc)
{
    if (ptenc->packet_size > 0)
        chafa_passthrough_encoder_flush (ptenc);

    chafa_term_info_unref (ptenc->term_info);
}

void
chafa_passthrough_encoder_append_len (ChafaPassthroughEncoder *ptenc, const gchar *in, gint len)
{
    if (ptenc->mode == CHAFA_PASSTHROUGH_NONE)
    {
        g_string_append_len (ptenc->out, in, len);
    }
    else if (ptenc->mode == CHAFA_PASSTHROUGH_SCREEN)
    {
        append_packetized (ptenc, in, len);
    }
    else
    {
        append_escaped (ptenc, in, len);
    }
}

void
chafa_passthrough_encoder_append (ChafaPassthroughEncoder *ptenc, const gchar *in)
{
    chafa_passthrough_encoder_append_len (ptenc, in, strlen (in));
}

void
chafa_passthrough_encoder_flush (ChafaPassthroughEncoder *ptenc)
{
    if (ptenc->packet_size > 0)
    {
        append_end (ptenc);
        ptenc->packet_size = 0;
    }
}

void
chafa_passthrough_encoder_reset (ChafaPassthroughEncoder *ptenc)
{
    ptenc->packet_size = 0;
}
