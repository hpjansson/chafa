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

#include "chafa.h"

#define BUFFER_SIZE_MAX 16384

typedef struct Buffer Buffer;

struct Buffer
{
    Buffer *next, *prev;
    gint ofs, len;
    guchar data [BUFFER_SIZE_MAX];
};

struct ChafaByteFifo
{
    Buffer *buf_head, *buf_tail;

    /* Stream position of first byte. Starts at 0. Increased by _pop(). */
    gint64 pos;

    /* Number of bytes in FIFO. */
    gint len;
};

static void
enqueue (ChafaByteFifo *fifo, gconstpointer src, gint src_len)
{
    const guchar *src_bytes = src;
    gint n;

    fifo->len += src_len;

    if (fifo->buf_tail)
    {
        n = MIN (src_len, BUFFER_SIZE_MAX - (fifo->buf_tail->ofs + fifo->buf_tail->len));
        memcpy (&fifo->buf_tail->data [fifo->buf_tail->ofs + fifo->buf_tail->len],
                src_bytes, n);
        fifo->buf_tail->len += n;
        src_bytes += n;
        src_len -= n;
    }

    while (src_len > 0)
    {
        Buffer *new_buf = g_new0 (Buffer, 1);

        if (!fifo->buf_head)
        {
            fifo->buf_head = fifo->buf_tail = new_buf;
        }
        else
        {
            fifo->buf_tail->next = new_buf;
            new_buf->prev = fifo->buf_tail;
            fifo->buf_tail = new_buf;
        }

        n = MIN (src_len, BUFFER_SIZE_MAX);
        memcpy (fifo->buf_tail->data, src_bytes, n);
        src_bytes += n;
        src_len -= n;
        fifo->buf_tail->len = n;
    }
}

/* dest can be NULL */
static gint
dequeue (ChafaByteFifo *fifo, gpointer dest, gint dest_len)
{
    guchar *dest_bytes = dest;
    Buffer *b;
    gint result_len;
    gint n;

    result_len = dest_len = MIN (dest_len, fifo->len);

    for (b = fifo->buf_head; dest_len > 0; )
    {
        n = MIN (dest_len, b->len);
        dest_len -= n;

        if (dest_bytes)
        {
            memcpy (dest_bytes, b->data + b->ofs, n);
            dest_bytes += n;
        }

        b->ofs += n;
        b->len -= n;

        if (b->len == 0)
        {
            g_assert (b->prev == NULL);

            fifo->buf_head = b->next;
            g_free (b);
            b = fifo->buf_head;
            if (b)
                b->prev = NULL;
        }
    }

    fifo->len -= result_len;
    if (fifo->len == 0)
        fifo->buf_tail = NULL;

    fifo->pos += result_len;
    return result_len;
}

static gint
fifo_search (ChafaByteFifo *fifo,
             gconstpointer data, gint data_len,
             gint64 *pos)
{
    Buffer *b, *match_b;
    const guint8 *data_u8 = data;
    gint data_ofs = 0;
    gint64 ofs = 0;
    gint64 buf_ofs;
    gint64 match_ofs, match_buf_ofs;
    gint result = -1;

    for (b = fifo->buf_head; b; b = b->next)
    {
        if (*pos < fifo->pos + ofs + b->len)
            break;

        ofs += b->len;
    }

    if (!b)
        goto out;

    buf_ofs = match_buf_ofs = *pos - (fifo->pos + ofs);
    match_b = b;
    match_ofs = ofs;

    for ( ; b; b = b->next)
    {
        for ( ; buf_ofs < b->len; buf_ofs++)
        {
            if (fifo->len - (ofs + buf_ofs) < data_len - data_ofs)
            {
                *pos = fifo->pos + ofs + buf_ofs;
                goto out;
            }

            if (data_u8 [data_ofs] == b->data [b->ofs + buf_ofs])
            {
                if (data_ofs == 0)
                {
                    match_b = b;
                    match_ofs = ofs;
                    match_buf_ofs = buf_ofs;
                }

                data_ofs++;
                if (data_ofs == data_len)
                {
                    *pos = fifo->pos + match_ofs + match_buf_ofs;
                    result = match_ofs + match_buf_ofs;
                    goto out;
                }
            }
            else if (data_ofs > 0)
            {
                data_ofs = 0;
                b = match_b;
                ofs = match_ofs;
                buf_ofs = match_buf_ofs;
            }
        }

        ofs += b->len;
        buf_ofs = 0;
    }

out:
    return result;
}

ChafaByteFifo *
chafa_byte_fifo_new (void)
{
    ChafaByteFifo *fifo;

    fifo = g_new0 (ChafaByteFifo, 1);
    return fifo;
}

void
chafa_byte_fifo_destroy (ChafaByteFifo *byte_fifo)
{
    g_return_if_fail (byte_fifo != NULL);

    g_free (byte_fifo);
}

gint64
chafa_byte_fifo_get_pos (ChafaByteFifo *byte_fifo)
{
    g_return_val_if_fail (byte_fifo != NULL, 0);

    return byte_fifo->pos;
}

gint
chafa_byte_fifo_get_len (ChafaByteFifo *byte_fifo)
{
    g_return_val_if_fail (byte_fifo != NULL, 0);

    return byte_fifo->len;
}

void
chafa_byte_fifo_push (ChafaByteFifo *byte_fifo, gconstpointer src, gint src_len)
{
    g_return_if_fail (byte_fifo != NULL);

    enqueue (byte_fifo, src, src_len);
}

gint
chafa_byte_fifo_pop (ChafaByteFifo *byte_fifo, gpointer dest, gint dest_len)
{
    g_return_val_if_fail (byte_fifo != NULL, 0);

    return dequeue (byte_fifo, dest, dest_len);
}

gconstpointer
chafa_byte_fifo_peek (ChafaByteFifo *byte_fifo, gint *len)
{
    Buffer *b;

    g_return_val_if_fail (byte_fifo != NULL, NULL);
    g_return_val_if_fail (len != NULL, NULL);

    *len = 0;

    if (byte_fifo->len == 0)
        return NULL;

    g_assert (byte_fifo->buf_head != NULL);

    for (b = byte_fifo->buf_head; ; b = b->next)
    {
        if (b->len > 0)
        {
            *len = b->len;
            return b->data + b->ofs;
        }
    }

    g_assert_not_reached ();
    return NULL;
}

gint
chafa_byte_fifo_drop (ChafaByteFifo *byte_fifo, gint len)
{
    g_return_val_if_fail (byte_fifo != NULL, 0);

    return dequeue (byte_fifo, NULL, len);
}

gint
chafa_byte_fifo_search (ChafaByteFifo *byte_fifo,
                        gconstpointer data, gint data_len,
                        gint64 *restart_pos)
{
    gint64 dummy_pos;

    g_return_val_if_fail (byte_fifo != NULL, -1);
    g_return_val_if_fail (data != NULL, -1);
    g_return_val_if_fail (data_len >= 0, -1);

    if (data_len == 0)
        return 0;

    if (!restart_pos)
    {
        dummy_pos = byte_fifo->pos;
        restart_pos = &dummy_pos;
    }
    else if (*restart_pos < byte_fifo->pos)
    {
        *restart_pos = byte_fifo->pos;
    }

    return fifo_search (byte_fifo, data, data_len, restart_pos);
}

gpointer
chafa_byte_fifo_split_next (ChafaByteFifo *byte_fifo,
                            gconstpointer separator, gint separator_len,
                            gint64 *restart_pos, gint *len_out)
{
    guint8 *data = NULL;
    gint len;

    g_return_val_if_fail (byte_fifo != NULL, NULL);
    g_return_val_if_fail (separator != NULL, NULL);
    g_return_val_if_fail (separator_len >= 0, NULL);

    len = chafa_byte_fifo_search (byte_fifo, separator, separator_len, restart_pos);
    if (len >= 0)
    {
        data = g_malloc (len + 1);
        chafa_byte_fifo_pop (byte_fifo, data, len);
        data [len] = '\0';
        chafa_byte_fifo_drop (byte_fifo, separator_len);
    }

    if (len_out)
        *len_out = len;
    return data;
}

#if 0

/* FIXME: Move this to tests/ once the API is made public */

static void
test_byte_fifo (void)
{
    ChafaByteFifo *byte_fifo;
    gchar buf [32768];
    gint result;

    memset (buf, 'x', 32768);

    byte_fifo = chafa_byte_fifo_new ();

    chafa_byte_fifo_push (byte_fifo, "abc", 3);
    result = chafa_byte_fifo_search (byte_fifo, "abc", 3, NULL);
    g_assert (result == 0);

    chafa_byte_fifo_drop (byte_fifo, 3);
    result = chafa_byte_fifo_search (byte_fifo, "abc", 3, NULL);
    g_assert (result == -1);

    chafa_byte_fifo_push (byte_fifo, "ababababcababab", 15);
    result = chafa_byte_fifo_search (byte_fifo, "abc", 3, NULL);
    g_assert (result == 6);

    chafa_byte_fifo_pop (byte_fifo, NULL, 1);
    result = chafa_byte_fifo_search (byte_fifo, "abc", 3, NULL);
    g_assert (result == 5);

    chafa_byte_fifo_push (byte_fifo, buf, 30000);
    result = chafa_byte_fifo_search (byte_fifo, "abc", 3, NULL);
    g_assert (result == 5);

    chafa_byte_fifo_drop (byte_fifo, 10);
    result = chafa_byte_fifo_search (byte_fifo, "abc", 3, NULL);
    g_assert (result == -1);

    chafa_byte_fifo_push (byte_fifo, "abc", 3);
    result = chafa_byte_fifo_search (byte_fifo, "abc", 3, NULL);
    g_assert (result == 30004);

    chafa_byte_fifo_drop (byte_fifo, 100000);
    result = chafa_byte_fifo_search (byte_fifo, "abc", 3, NULL);
    g_assert (result == -1);

    chafa_byte_fifo_push (byte_fifo, buf, 16380);
    chafa_byte_fifo_push (byte_fifo, "abracadabra", 11);
    result = chafa_byte_fifo_search (byte_fifo, "abracadabra", 11, NULL);
    g_assert (result == 16380);

    chafa_byte_fifo_drop (byte_fifo, 100000);
    chafa_byte_fifo_push (byte_fifo, buf, 16380);
    chafa_byte_fifo_push (byte_fifo, "abracadfrumpy", 13);
    result = chafa_byte_fifo_search (byte_fifo, "abracadabra", 11, NULL);
    g_assert (result == -1);

    chafa_byte_fifo_push (byte_fifo, "abracadabra", 11);
    result = chafa_byte_fifo_search (byte_fifo, "abracadabra", 11, NULL);
    g_assert (result == 16393);
}

#endif

