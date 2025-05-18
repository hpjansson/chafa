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

#include "chafa-byte-fifo.h"

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

    return result_len;
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
