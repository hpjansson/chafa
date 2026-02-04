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
#include <glib/gprintf.h>  /* g_vasprintf */

/* Our copy of glib's internal GWakeup */
#include "chafa-wakeup.h"

#include "chafa.h"
#include "chafa-byte-fifo.h"
#include "chafa-stream-writer.h"

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
#define WRITE_BUF_MAX 4096

/* Max fifo size before forced sync */
#define FIFO_DEFAULT_MAX (1 << 20)

struct ChafaStreamWriter
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
    DWORD saved_console_mode;
#endif
    gint fd;
    gint buf_max;

    guint is_console : 1;
    guint drained : 1;
    guint shutdown_reqd : 1;
    guint shutdown_done : 1;
};

/* ------------------ *
 * MS Windows helpers *
 * ------------------ */

#ifdef G_OS_WIN32

static void
win32_stream_writer_init (ChafaStreamWriter *stream_writer)
{
    GetConsoleMode (stream_writer->fd_win32, &stream_writer->saved_console_mode);

    setmode (stream_writer->fd, O_BINARY);

    if (SetConsoleMode (stream_writer->fd_win32,
                        ENABLE_PROCESSED_OUTPUT
                        | ENABLE_VIRTUAL_TERMINAL_PROCESSING
                        | ENABLE_WRAP_AT_EOL_OUTPUT))
    {
        stream_writer->is_console = TRUE;
    }
    else
    {
        if (GetLastError () != ERROR_INVALID_HANDLE)
        {
            /* Legacy MS Windows */
            stream_writer->is_console = TRUE;
            SetConsoleMode (stream_writer->fd_win32, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT);
        }
    }
}

static void
win32_stream_writer_deinit (ChafaStreamWriter *stream_writer)
{
    SetConsoleMode (stream_writer->fd_win32, stream_writer->saved_console_mode);
}

static gboolean
safe_WriteConsoleA (ChafaStreamWriter *stream_writer, const gchar *data, gsize len)
{
    gsize total_written = 0;

    g_assert (stream_writer->fd_win32 != INVALID_HANDLE_VALUE);

    while (total_written < len)
    {
        DWORD n_written = 0;

        if (stream_writer->is_console)
        {
            if (!WriteConsoleA (stream_writer->fd_win32, data, len - total_written, &n_written, NULL))
                return FALSE;
        }
        else
        {
            /* WriteFile() and fwrite() seem to work equally well despite various
             * claims that the former does poorly in a UTF-8 environment. The
             * resulting files look good in my tests, but note that catting them
             * out with 'type' introduces lots of artefacts. */

            if (!WriteFile (stream_writer->fd_win32, data, len - total_written, &n_written, NULL))
                return FALSE;
        }

        data += n_written;
        total_written += n_written;
    }

    return TRUE;
}

#else /* !G_OS_WIN32 */

static gboolean
wait_for_pipe (gint fd)
{
    GPollFD poll_fds [1];

    poll_fds [0].fd = fd;
    poll_fds [0].events = G_IO_OUT | G_IO_HUP | G_IO_ERR;
    poll_fds [0].revents = 0;

    g_poll (poll_fds, 1, -1);

    return (poll_fds [0].revents & (G_IO_HUP | G_IO_ERR)) ? FALSE : TRUE;
}

static gboolean
safe_write (gint fd, gconstpointer buf, gsize len)
{
    const guint8 *buffer = buf;
    gboolean success = FALSE;

    while (len > 0)
    {
       guint to_write;
       gint n_written;
       gint saved_errno;

       if (len > INT_MAX)
           to_write = INT_MAX;
       else
           to_write = (unsigned int) len;

       n_written = write (fd, buffer, to_write);
       saved_errno = errno;

       if (n_written == -1)
       {
           if (saved_errno == EAGAIN)
           {
# ifdef __gnu_hurd__
               /* On GNU/Hurd we get EAGAIN if the remote end closed the pipe,
                * and if the pipe is made blocking it simply stalls. This makes
                * our >&- redirection test fail. Therefore we bail out here
                * as the least bad option. */
               goto out;
# else
               /* It's a nonblocking pipe; wait for it to become ready,
                * then try again. */
               if (!wait_for_pipe (fd))
                   goto out;
# endif
           }
           else if (saved_errno != EINTR)
           {
               goto out;
           }
       }
       else if (n_written < 0)
       {
           /* Not a valid 'write' result */
           goto out;
       }
       else if (n_written > 0)
       {
           /* Continue writing until permanent failure or entire buffer written */
           buffer += n_written;
           len -= (unsigned int) n_written;
       }
    }

    success = TRUE;

out:
    return success;
}

#endif

/* -------------------------------- *
 * Low-level I/O and tty whispering *
 * -------------------------------- */

static gboolean
write_to_stream (ChafaStreamWriter *stream_writer, gconstpointer buf, gsize len)
{
    gboolean result = TRUE;

    if (len == 0)
        return TRUE;

#ifdef G_OS_WIN32
    {
        gsize total_written = 0;
        const void * const newline = "\r\n";
        const gchar *p0, *p1, *end;

        /* On MS Windows, we convert line feeds to DOS-style CRLF as we go. */

        for (p0 = buf, end = p0 + len;
             stream_writer->fd_win32 != INVALID_HANDLE_VALUE && total_written < len;
             p0 = p1)
        {
            p1 = memchr (p0, '\n', end - p0);
            if (!p1)
                p1 = end;

            if (!safe_WriteConsoleA (stream_writer, p0, p1 - p0))
                break;

            total_written += p1 - p0;

            if (p1 != end)
            {
                if (!safe_WriteConsoleA (stream_writer, newline, 2))
                    break;

                p1 += 1;
                total_written += 1;
            }
        }

        result = total_written == len ? TRUE : FALSE;
    }
#else
    {
        result = safe_write (stream_writer->fd, buf, len);
    }
#endif

    return result;
}

/* ----------------------- *
 * Mid-level I/O machinery *
 * ----------------------- */

static gpointer
thread_main (gpointer data)
{
    ChafaStreamWriter *stream_writer = data;
    gboolean io_error = FALSE;

    for (;;)
    {
        guchar buf [WRITE_BUF_MAX];
        gint len;

        g_mutex_lock (&stream_writer->mutex);

        if (io_error || stream_writer->shutdown_reqd)
            break;

        if (!chafa_byte_fifo_get_len (stream_writer->fifo))
        {
            /* Pending output has now left the process. Signal main thread; it
             * may be waiting to finish a flush */
            stream_writer->drained = TRUE;
            g_cond_broadcast (&stream_writer->cond);
        }

        len = 0;

        while (!stream_writer->shutdown_reqd)
        {
            len = chafa_byte_fifo_pop (stream_writer->fifo, buf, WRITE_BUF_MAX);
            if (len)
                break;

            g_cond_wait (&stream_writer->cond, &stream_writer->mutex);
        }

        g_mutex_unlock (&stream_writer->mutex);

        if (!write_to_stream (stream_writer, buf, len))
            io_error = TRUE;
    }

    stream_writer->shutdown_done = TRUE;
    g_cond_broadcast (&stream_writer->cond);
    g_mutex_unlock (&stream_writer->mutex);
    return NULL;
}

static void
maybe_start_thread (ChafaStreamWriter *stream_writer)
{
    if (stream_writer->thread)
        return;

    stream_writer->thread = g_thread_new ("stream-writer", thread_main, stream_writer);
}

/* --------------------- *
 * Construct and destroy *
 * --------------------- */

static void
chafa_stream_writer_init (ChafaStreamWriter *stream_writer, gint fd)
{
    stream_writer->refs = 1;
    stream_writer->fd = fd;
    stream_writer->buf_max = FIFO_DEFAULT_MAX;
    stream_writer->fifo = chafa_byte_fifo_new ();
    stream_writer->wakeup = chafa_wakeup_new ();
    g_mutex_init (&stream_writer->mutex);
    g_cond_init (&stream_writer->cond);

#ifdef G_OS_WIN32
    stream_writer->fd_win32 = (HANDLE) _get_osfhandle (stream_writer->fd);
    win32_stream_writer_init (stream_writer);
#else
    if (isatty (stream_writer->fd))
        stream_writer->is_console = TRUE;
#endif
}

static void
chafa_stream_writer_destroy (ChafaStreamWriter *stream_writer)
{
    g_return_if_fail (stream_writer != NULL);

    g_mutex_lock (&stream_writer->mutex);

    stream_writer->shutdown_reqd = TRUE;
    chafa_wakeup_signal (stream_writer->wakeup);
    g_cond_broadcast (&stream_writer->cond);

    while (stream_writer->thread && !stream_writer->shutdown_done)
        g_cond_wait (&stream_writer->cond, &stream_writer->mutex);

    g_mutex_unlock (&stream_writer->mutex);

    if (stream_writer->thread)
        g_thread_join (stream_writer->thread);

#ifdef G_OS_WIN32
    win32_stream_writer_deinit (stream_writer);
#endif

    g_mutex_clear (&stream_writer->mutex);
    g_cond_clear (&stream_writer->cond);
    chafa_wakeup_free (stream_writer->wakeup);

    chafa_byte_fifo_destroy (stream_writer->fifo);

    g_free (stream_writer);
}

/* ---------- *
 * Public API *
 * ---------- */

ChafaStreamWriter *
chafa_stream_writer_new_from_fd (gint fd)
{
    ChafaStreamWriter *stream_writer;

    g_return_val_if_fail (fd >= 0, NULL);

    stream_writer = g_new0 (ChafaStreamWriter, 1);
    chafa_stream_writer_init (stream_writer, fd);

    return stream_writer;
}

void
chafa_stream_writer_ref (ChafaStreamWriter *stream_writer)
{
    gint refs;

    g_return_if_fail (stream_writer != NULL);
    refs = g_atomic_int_get (&stream_writer->refs);
    g_return_if_fail (refs > 0);

    g_atomic_int_inc (&stream_writer->refs);
}

void
chafa_stream_writer_unref (ChafaStreamWriter *stream_writer)
{
    gint refs;

    g_return_if_fail (stream_writer != NULL);
    refs = g_atomic_int_get (&stream_writer->refs);
    g_return_if_fail (refs > 0);

    if (g_atomic_int_dec_and_test (&stream_writer->refs))
    {
        chafa_stream_writer_destroy (stream_writer);
    }
}

gint
chafa_stream_writer_get_fd (ChafaStreamWriter *stream_writer)
{
    g_return_val_if_fail (stream_writer != NULL, -1);

    return stream_writer->fd;
}

gboolean
chafa_stream_writer_is_console (ChafaStreamWriter *stream_writer)
{
    g_return_val_if_fail (stream_writer != NULL, FALSE);

    return stream_writer->is_console;
}

gint
chafa_stream_writer_get_buffer_max (ChafaStreamWriter *stream_writer)
{
    g_return_val_if_fail (stream_writer != NULL, -1);

    return stream_writer->buf_max;
}

void
chafa_stream_writer_set_buffer_max (ChafaStreamWriter *stream_writer, gint buf_max)
{
    g_return_if_fail (stream_writer != NULL);
    g_return_if_fail (buf_max > 0);

    stream_writer->buf_max = buf_max;
}

void
chafa_stream_writer_write (ChafaStreamWriter *stream_writer, gconstpointer data, gint len)
{
    maybe_start_thread (stream_writer);

    while (len > 0)
    {
        gint n_written;

        g_mutex_lock (&stream_writer->mutex);

        /* Wait for partial drain if necessary */

        for (;;)
        {
            gint queued = chafa_byte_fifo_get_len (stream_writer->fifo);

            if (queued == 0 || queued + len <= stream_writer->buf_max)
                break;

            if (stream_writer->shutdown_done)
            {
                g_mutex_unlock (&stream_writer->mutex);
                goto out;
            }

            g_cond_wait (&stream_writer->cond, &stream_writer->mutex);
        }

        /* Push and signal */

        stream_writer->drained = FALSE;
        n_written = MIN (len, stream_writer->buf_max);

        chafa_byte_fifo_push (stream_writer->fifo, data, n_written);

        len -= n_written;
        data = ((const gchar *) data) + n_written;

        g_cond_broadcast (&stream_writer->cond);
        g_mutex_unlock (&stream_writer->mutex);
    }

out:
    return;
}

gint
chafa_stream_writer_print (ChafaStreamWriter *stream_writer, const gchar *format, ...)
{
    gchar *str = NULL;
    va_list args;
    gint result;

    va_start (args, format);
    result = g_vasprintf (&str, format, args);
    va_end (args);

    if (result > 0)
        chafa_stream_writer_write (stream_writer, str, result);

    g_free (str);
    return result;
}

gboolean
chafa_stream_writer_flush (ChafaStreamWriter *stream_writer)
{
    maybe_start_thread (stream_writer);

    g_mutex_lock (&stream_writer->mutex);
    while (!stream_writer->shutdown_done && !stream_writer->drained)
        g_cond_wait (&stream_writer->cond, &stream_writer->mutex);
    g_mutex_unlock (&stream_writer->mutex);

    return TRUE;
}
