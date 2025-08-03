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

#include <sys/types.h>  /* open */
#include <sys/stat.h>  /* stat */
#include <fcntl.h>  /* open */
#include <unistd.h>  /* STDOUT_FILENO */

/* Our copy of glib's internal GWakeup */
#include "chafa-wakeup.h"

#include "chafa.h"
#include "chafa-byte-fifo.h"
#include "chafa-stream-reader.h"

/* Include after glib.h for G_OS_WIN32 */
#ifdef G_OS_WIN32
# ifdef HAVE_WINDOWS_H
#  include <windows.h>
# endif
# include <wchar.h>
# include <io.h>
#else
# include <glib-unix.h>
#endif

/* ------------------- *
 * Defines and structs *
 * ------------------- */

/* Stack buffer size */
#define READ_BUF_MAX 4096

/* Max fifo size before forced sync */
#define FIFO_DEFAULT_MAX 32768

struct ChafaStreamReader
{
    gint refs;

    GThread *thread;
    ChafaByteFifo *fifo;
    GMutex mutex;
    GCond cond;
    ChafaWakeup *wakeup;
    GQueue *event_queue;
#ifdef G_OS_WIN32
    HANDLE fd_win32;
#endif

    gint64 token_restart_pos;
    gpointer token_separator;
    gint token_separator_len;

    gint fd;
    guint idle_id;
    gint buf_max;

    guint is_console : 1;

    /* TRUE if EOF event was seen on input FD */
    guint eof_seen : 1;

    guint shutdown_reqd : 1;
    guint shutdown_done : 1;
};

/* -------------------------------- *
 * Low-level I/O and tty whispering *
 * -------------------------------- */

static gint
read_from_stream (ChafaStreamReader *stream_reader, guchar *out, gint max)
{
    GPollFD poll_fds [2];
    gint result = -1;

    if (stream_reader->fd < 0)
        goto out;

#ifdef G_OS_WIN32
    poll_fds [0].fd = (gintptr) stream_reader->fd_win32;
#else
    poll_fds [0].fd = stream_reader->fd;
#endif
    poll_fds [0].events = G_IO_IN | G_IO_HUP | G_IO_ERR;
    poll_fds [0].revents = 0;

    poll_fds [1].revents = 0;
    chafa_wakeup_get_pollfd (stream_reader->wakeup, &poll_fds [1]);

    g_poll (poll_fds, 2, -1);

    /* Check for wakeup call; this means we should exit immediately */
    if (poll_fds [1].revents)
        goto out;

    if (poll_fds [0].revents & G_IO_IN)
    {
#ifdef G_OS_WIN32
        DWORD n_read = 0;
        gboolean op_success = FALSE;

        if (stream_reader->is_console)
            op_success = ReadConsoleA (stream_reader->fd_win32, out, max, &n_read, NULL);
        else
            op_success = ReadFile (stream_reader->fd_win32, out, max, &n_read, NULL);

        if (op_success)
            result = n_read;
        else if (GetLastError () != ERROR_IO_PENDING)
            result = -1;
#else /* !G_OS_WIN32 */
        /* Non-blocking read */
        result = read (stream_reader->fd, out, max);
        if (result < 1)
        {
            result = (errno == EAGAIN || errno == EINTR) ? 0 : -1;
        }
#endif
    }
    else if (poll_fds [0].revents & (G_IO_HUP | G_IO_ERR))
    {
        result = -1;
    }

out:
    return result;
}

/* ----------------------- *
 * Mid-level I/O machinery *
 * ----------------------- */

static gboolean
in_idle_func (gpointer data)
{
    ChafaStreamReader *stream_reader = data;

    /* Dispatch read events in main thread */

    g_mutex_lock (&stream_reader->mutex);
    stream_reader->idle_id = 0;

    for (;;)
    {
        guchar buf [READ_BUF_MAX];
        gint len;

        /* FIXME: Can use peek here and save copying */
        len = chafa_byte_fifo_pop (stream_reader->fifo, buf, READ_BUF_MAX);
        if (len < 1)
            break;

        /* TODO: Do something! */
    }

    g_mutex_unlock (&stream_reader->mutex);

    /* TODO: Actually dispatch the events from parser */

    return G_SOURCE_REMOVE;
}

static gpointer
thread_main (gpointer data)
{
    ChafaStreamReader *stream_reader = data;

    for (;;)
    {
        guchar buf [READ_BUF_MAX];
        gint len;

        len = read_from_stream (stream_reader, buf, READ_BUF_MAX);

        g_mutex_lock (&stream_reader->mutex);

        if (len < 0)
            stream_reader->eof_seen = TRUE;

        if (stream_reader->eof_seen || stream_reader->shutdown_reqd)
        {
            break;
        }
        else if (len > 0)
        {
            chafa_byte_fifo_push (stream_reader->fifo, buf, len);
            g_cond_broadcast (&stream_reader->cond);
            if (!stream_reader->idle_id)
                stream_reader->idle_id = g_idle_add (in_idle_func, stream_reader);
        }

        while (chafa_byte_fifo_get_len (stream_reader->fifo) > FIFO_DEFAULT_MAX
               && !stream_reader->shutdown_reqd)
            g_cond_wait (&stream_reader->cond, &stream_reader->mutex);

        if (stream_reader->shutdown_reqd)
            break;

        g_mutex_unlock (&stream_reader->mutex);
    }

    stream_reader->shutdown_done = TRUE;
    g_cond_broadcast (&stream_reader->cond);
    g_mutex_unlock (&stream_reader->mutex);
    return NULL;
}

static void
maybe_start_thread (ChafaStreamReader *stream_reader)
{
    if (stream_reader->thread)
        return;

    /* Reader thread sits in poll() and does non-blocking reads */
#ifndef G_OS_WIN32
    g_unix_set_fd_nonblocking (stream_reader->fd, TRUE, NULL);
#endif
    stream_reader->thread = g_thread_new ("stream-reader", thread_main, stream_reader);
}

static gboolean
is_eof_unlocked (ChafaStreamReader *stream_reader)
{
    return chafa_byte_fifo_get_len (stream_reader->fifo) == 0
        && (stream_reader->eof_seen || stream_reader->shutdown_done);
}

/* --------------------- *
 * Construct and destroy *
 * --------------------- */

static void
chafa_stream_reader_init (ChafaStreamReader *stream_reader, gint fd,
                          gconstpointer token_separator, gint token_separator_len)
{
    stream_reader->refs = 1;
    stream_reader->fd = fd;
    stream_reader->buf_max = FIFO_DEFAULT_MAX;
    stream_reader->fifo = chafa_byte_fifo_new ();
    stream_reader->wakeup = chafa_wakeup_new ();
    g_mutex_init (&stream_reader->mutex);
    g_cond_init (&stream_reader->cond);

    if (token_separator && token_separator_len > 0)
    {
        stream_reader->token_separator = g_memdup (token_separator, token_separator_len);
        stream_reader->token_separator_len = token_separator_len;
    }
    else
    {
        stream_reader->token_separator = g_memdup ("", 1);
        stream_reader->token_separator_len = 1;
    }

#ifdef G_OS_WIN32
    stream_reader->fd_win32 = (HANDLE) _get_osfhandle (stream_reader->fd);
    setmode (stream_reader->fd, O_BINARY);

    if (SetConsoleMode (stream_reader->fd_win32,
                        ENABLE_PROCESSED_INPUT
                        | ENABLE_VIRTUAL_TERMINAL_INPUT))
    {
        stream_reader->is_console = TRUE;
    }
    else
    {
        if (GetLastError () != ERROR_INVALID_HANDLE)
        {
            /* Legacy MS Windows */
            stream_reader->is_console = TRUE;
        }
    }
#else
    if (isatty (stream_reader->fd))
        stream_reader->is_console = TRUE;
#endif
}

static void
chafa_stream_reader_destroy (ChafaStreamReader *stream_reader)
{
    g_return_if_fail (stream_reader != NULL);

    g_mutex_lock (&stream_reader->mutex);

    if (stream_reader->idle_id)
    {
        g_source_remove (stream_reader->idle_id);
        stream_reader->idle_id = 0;
    }

    stream_reader->shutdown_reqd = TRUE;
    chafa_wakeup_signal (stream_reader->wakeup);
    g_cond_broadcast (&stream_reader->cond);

    while (stream_reader->thread && !stream_reader->shutdown_done)
        g_cond_wait (&stream_reader->cond, &stream_reader->mutex);

    g_mutex_unlock (&stream_reader->mutex);

    if (stream_reader->thread)
        g_thread_join (stream_reader->thread);

    g_mutex_clear (&stream_reader->mutex);
    g_cond_clear (&stream_reader->cond);
    chafa_wakeup_free (stream_reader->wakeup);

    chafa_byte_fifo_destroy (stream_reader->fifo);
    g_free (stream_reader->token_separator);

    g_free (stream_reader);
}

/* ---------- *
 * Public API *
 * ---------- */

ChafaStreamReader *
chafa_stream_reader_new_from_fd (gint fd)
{
    ChafaStreamReader *stream_reader;

    g_return_val_if_fail (fd >= 0, NULL);

    stream_reader = g_new0 (ChafaStreamReader, 1);
    chafa_stream_reader_init (stream_reader, fd, NULL, -1);

    return stream_reader;
}

ChafaStreamReader *
chafa_stream_reader_new_from_fd_full (gint fd, gconstpointer token_separator,
                                      gint token_separator_len)
{
    ChafaStreamReader *stream_reader;

    g_return_val_if_fail (fd >= 0, NULL);

    stream_reader = g_new0 (ChafaStreamReader, 1);
    chafa_stream_reader_init (stream_reader, fd, token_separator, token_separator_len);

    return stream_reader;
}

void
chafa_stream_reader_ref (ChafaStreamReader *stream_reader)
{
    gint refs;

    g_return_if_fail (stream_reader != NULL);
    refs = g_atomic_int_get (&stream_reader->refs);
    g_return_if_fail (refs > 0);

    g_atomic_int_inc (&stream_reader->refs);
}

void
chafa_stream_reader_unref (ChafaStreamReader *stream_reader)
{
    gint refs;

    g_return_if_fail (stream_reader != NULL);
    refs = g_atomic_int_get (&stream_reader->refs);
    g_return_if_fail (refs > 0);

    if (g_atomic_int_dec_and_test (&stream_reader->refs))
    {
        chafa_stream_reader_destroy (stream_reader);
    }
}

gint
chafa_stream_reader_get_fd (ChafaStreamReader *stream_reader)
{
    g_return_val_if_fail (stream_reader != NULL, -1);

    return stream_reader->fd;
}

gboolean
chafa_stream_reader_is_console (ChafaStreamReader *stream_reader)
{
    g_return_val_if_fail (stream_reader != NULL, FALSE);

    return stream_reader->is_console;
}

static void
popped_fifo (ChafaStreamReader *stream_reader)
{
    /* If there's space in the buffer, tell thread to resume reading */
    if (chafa_byte_fifo_get_len (stream_reader->fifo) <= FIFO_DEFAULT_MAX)
        g_cond_broadcast (&stream_reader->cond);
}

gint
chafa_stream_reader_read (ChafaStreamReader *stream_reader, gpointer out, gint max_len)
{
    gint result = -1;

    g_return_val_if_fail (stream_reader != NULL, -1);

    maybe_start_thread (stream_reader);

    g_mutex_lock (&stream_reader->mutex);
    result = chafa_byte_fifo_pop (stream_reader->fifo, out, max_len);
    popped_fifo (stream_reader);
    g_mutex_unlock (&stream_reader->mutex);

    return result;
}

/* FIXME: Honor max_len, both for a regular split and remainder. Return an
 * error code on oversized tokens, and skip over them. */
gint
chafa_stream_reader_read_token (ChafaStreamReader *stream_reader, gpointer *out, gint max_len)
{
    gchar *token = NULL;
    gint result = -1;

    g_return_val_if_fail (stream_reader != NULL, -1);

    maybe_start_thread (stream_reader);

    g_mutex_lock (&stream_reader->mutex);

    token = chafa_byte_fifo_split_next (stream_reader->fifo,
                                        stream_reader->token_separator,
                                        stream_reader->token_separator_len,
                                        &stream_reader->token_restart_pos,
                                        &result);
    if (!token && is_eof_unlocked (stream_reader))
    {
        gint len = chafa_byte_fifo_get_len (stream_reader->fifo);

        /* Return remaining data after final separator */

        if (len > 0)
        {
            token = g_malloc (len + 1);
            chafa_byte_fifo_pop (stream_reader->fifo, token, len);
            token [len] = '\0';
            result = len;
        }
    }

    popped_fifo (stream_reader);
    g_mutex_unlock (&stream_reader->mutex);

    if (result >= 0)
        *out = token;
    return result;
}

static gboolean
chafa_stream_reader_wait_until_locked (ChafaStreamReader *stream_reader, gint64 end_time_us)
{
    return g_cond_wait_until (&stream_reader->cond, &stream_reader->mutex, end_time_us);
}

gboolean
chafa_stream_reader_wait_until (ChafaStreamReader *stream_reader, gint64 end_time_us)
{
    gboolean result = FALSE;

    g_return_val_if_fail (stream_reader != NULL, FALSE);

    if (end_time_us < 1 || end_time_us <= g_get_monotonic_time ())
        return FALSE;

    maybe_start_thread (stream_reader);

    g_mutex_lock (&stream_reader->mutex);
    result = chafa_stream_reader_wait_until_locked (stream_reader, end_time_us);
    g_mutex_unlock (&stream_reader->mutex);

    return result;
}

void
chafa_stream_reader_wait (ChafaStreamReader *stream_reader, gint timeout_ms)
{
    g_return_if_fail (stream_reader != NULL);

    maybe_start_thread (stream_reader);

    g_mutex_lock (&stream_reader->mutex);

    if (stream_reader->shutdown_done)
    {
        g_mutex_unlock (&stream_reader->mutex);
        return;
    }

    if (timeout_ms > 0)
    {
        gint64 end_time_us = g_get_monotonic_time () + timeout_ms * 1000;
        chafa_stream_reader_wait_until_locked (stream_reader, end_time_us);
    }
    else
    {
        g_cond_wait (&stream_reader->cond, &stream_reader->mutex);
    }

    g_mutex_unlock (&stream_reader->mutex);
}

gboolean
chafa_stream_reader_is_eof (ChafaStreamReader *stream_reader)
{
    gboolean is_eof = FALSE;

    g_return_val_if_fail (stream_reader != NULL, TRUE);

    g_mutex_lock (&stream_reader->mutex);
    is_eof = is_eof_unlocked (stream_reader);
    g_mutex_unlock (&stream_reader->mutex);

    return is_eof;
}
