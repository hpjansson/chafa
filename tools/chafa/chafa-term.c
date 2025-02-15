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

#include "config.h"

#include <stdio.h>
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>  /* ioctl */
#endif
#ifdef HAVE_TERMIOS_H
# include <termios.h>  /* tcgetattr, tcsetattr */
#endif
#include <sys/types.h>  /* open */
#include <sys/stat.h>  /* stat */
#include <fcntl.h>  /* open */
#include <unistd.h>  /* STDOUT_FILENO */
#include <glib/gstdio.h>

/* Our copy of glib's internal GWakeup */
#include "chafa-wakeup.h"

#include "chafa.h"
#include "chafa-byte-fifo.h"
#include "chafa-term.h"

/* Include after glib.h for G_OS_WIN32 */
#ifdef G_OS_WIN32
# ifdef HAVE_WINDOWS_H
#  include <windows.h>
# endif
# include <wchar.h>
# include <io.h>
# include "conhost.h"
#else
# include <glib-unix.h>
#endif

/* ------------------- *
 * Defines and structs *
 * ------------------- */

/* Maximum width or height of the terminal, in pixels. If it claims to be
 * bigger than this, assume it's broken. */
#define PIXEL_EXTENT_MAX (8192 * 3)

/* Stack buffer size */
#define READ_BUF_MAX 4096
#define WRITE_BUF_MAX 4096

/* Max fifo size before forced sync */
#define IN_FIFO_DEFAULT_MAX 16384
#define OUT_FIFO_DEFAULT_MAX (1 << 20)

struct ChafaTerm
{
    ChafaTermInfo *term_info;
    ChafaTermInfo *default_term_info;
    ChafaParser *parser;

    gint width_cells, height_cells;
    gint width_px, height_px;

    /* TRUE if we probed the tty size at least once */
    guint have_tty_size : 1;

    /* TRUE if both input and output fds are connected to a terminal */
    guint interactive_supported : 1;

    /* TRUE if EOF event was seen on input FD */
    guint in_eof_seen : 1;

    /* TRUE if probe query was sent */
    guint probe_attempt : 1;

    /* TRUE if probe response was received */
    guint probe_success : 1;

    /* TRUE if sixel capability was detected by the last probe */
    guint probe_found_sixel : 1;

    /* I/O bookkeeping */

    GThread *in_thread, *out_thread;
    ChafaByteFifo *in_fifo, *out_fifo, *err_fifo;
    GMutex mutex;
    GCond cond;
    ChafaWakeup *wakeup;
    GQueue *event_queue;
    guint in_idle_id;
    gint in_fd, out_fd, err_fd;
    gint in_buf_max, out_buf_max;

    guint out_drained : 1;
    guint shutdown_reqd : 1;
    guint in_shutdown_done : 1;
    guint out_shutdown_done : 1;
};

/* -------------------------------- *
 * Low-level I/O and tty whispering *
 * -------------------------------- */

static gsize
safe_read (gint fd, void *buf, gsize len)
{
   gsize ntotal = 0;
   guint8 *buffer = buf;

   while (len > 0)
   {
       unsigned int nread;
       int iread;
       int saved_errno;

       /* Passing nread > INT_MAX to read is implementation defined in POSIX
        * 1003.1, therefore despite the unsigned argument portable code must
        * limit the value to INT_MAX.
        */
       if (len > INT_MAX)
           nread = INT_MAX;
       else
           nread = (unsigned int)/*SAFE*/len;

       iread = read (fd, buffer, nread);
       saved_errno = errno;

       if (iread == -1)
       {
           /* A read can terminate early with 0 bytes read because of EINTR,
            * yet it still returns -1 otherwise end of file cannot be distinguished. */
           if (saved_errno != EINTR)
           {
               /* I.e. a permanent failure */
               return 0;
           }
       }
       else if (iread < 0)
       {
           /* Not a valid 'read' result: */
           return 0;
       }
       else if (iread > 0)
       {
           /* Continue reading until a permanent failure, or EOF */
           buffer += iread;
           len -= (unsigned int)/*SAFE*/iread;
           ntotal += (unsigned int)/*SAFE*/iread;
       }
       else
       {
           return ntotal;
       }
   }

   return ntotal; /* len == 0 */
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
           if (saved_errno != EINTR)
               goto out;
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

static gboolean
write_to_stderr (ChafaTerm *term, gconstpointer buf, gsize len)
{
    gboolean result = TRUE;

    if (len == 0)
        return TRUE;

#ifdef G_OS_WIN32
    {
        HANDLE chd = GetStdHandle (STD_ERROR_HANDLE);
        gsize total_written = 0;
        const void * const newline = "\r\n";
        const gchar *p0, *p1, *end;

        /* On MS Windows, we convert line feeds to DOS-style CRLF as we go. */

        for (p0 = buf, end = p0 + len;
             chd != INVALID_HANDLE_VALUE && total_written < len;
             p0 = p1)
        {
            p1 = memchr (p0, '\n', end - p0);
            if (!p1)
                p1 = end;

            if (!safe_WriteConsoleA (chd, p0, p1 - p0))
                break;

            total_written += p1 - p0;

            if (p1 != end)
            {
                if (!safe_WriteConsoleA (chd, newline, 2))
                    break;

                p1 += 1;
                total_written += 1;
            }
        }

        result = total_written == len ? TRUE : FALSE;
    }
#else
    {
        result = safe_write (term->err_fd, buf, len);
    }
#endif

    return result;
}

static gboolean
write_to_stdout (ChafaTerm *term, gconstpointer buf, gsize len)
{
    gboolean result = TRUE;

    if (len == 0)
        return TRUE;

#ifdef G_OS_WIN32
    {
        HANDLE chd = GetStdHandle (STD_OUTPUT_HANDLE);
        gsize total_written = 0;
        const void * const newline = "\r\n";
        const gchar *p0, *p1, *end;

        /* On MS Windows, we convert line feeds to DOS-style CRLF as we go. */

        for (p0 = buf, end = p0 + len;
             chd != INVALID_HANDLE_VALUE && total_written < len;
             p0 = p1)
        {
            p1 = memchr (p0, '\n', end - p0);
            if (!p1)
                p1 = end;

            if (!safe_WriteConsoleA (chd, p0, p1 - p0))
                break;

            total_written += p1 - p0;

            if (p1 != end)
            {
                if (!safe_WriteConsoleA (chd, newline, 2))
                    break;

                p1 += 1;
                total_written += 1;
            }
        }

        result = total_written == len ? TRUE : FALSE;
    }
#else
    {
        result = safe_write (term->out_fd, buf, len);
    }
#endif

    return result;
}

static gint
read_from_stdin (ChafaTerm *term, guchar *out, gint max)
{
    GPollFD poll_fds [2];
    gint result = -1;

    if (term->in_fd < 0)
        goto out;

#ifdef G_OS_WIN32
    poll_fds [0].fd = (gintptr) GetStdHandle (STD_INPUT_HANDLE);
#else
    poll_fds [0].fd = term->in_fd;
#endif
    poll_fds [0].events = G_IO_IN | G_IO_HUP | G_IO_ERR;
    poll_fds [0].revents = 0;

    poll_fds [1].revents = 0;
    chafa_wakeup_get_pollfd (term->wakeup, &poll_fds [1]);

    g_poll (poll_fds, 2, -1);

    /* Check for wakeup call; this means we should exit immediately */
    if (poll_fds [1].revents)
        goto out;

    if (poll_fds [0].revents & G_IO_IN)
    {
#ifdef G_OS_WIN32
        {
            HANDLE chd = GetStdHandle (STD_INPUT_HANDLE);
            DWORD n_read = 0;

            ReadConsoleA (chd, out, max, &n_read, NULL);
            result = n_read;
        }
#else /* !G_OS_WIN32 */
        {
            /* Non-blocking read */
            result = read (term->in_fd, out, max);
        }
#endif
    }

out:
    return result;
}

#ifdef HAVE_TERMIOS_H
static void
ensure_raw_mode_enabled (ChafaTerm *term, struct termios *saved_termios,
                         gboolean *termios_changed)
{
    struct termios t;

    tcgetattr (term->in_fd, saved_termios);

    t = *saved_termios;
    t.c_lflag &= ~(ECHO | ICANON);

    if (t.c_lflag != saved_termios->c_lflag)
    {
        *termios_changed = TRUE;
        tcsetattr (term->in_fd, TCSANOW, &t);
    }
    else
    {
        *termios_changed = FALSE;
    }
}
#endif

#ifdef HAVE_TERMIOS_H
static void
restore_termios (ChafaTerm *term, const struct termios *saved_termios, gboolean *termios_changed)
{
    if (!*termios_changed)
        return;

    tcsetattr (term->in_fd, TCSANOW, saved_termios);
}
#endif

static void
get_tty_size (ChafaTerm *term)
{
    term->width_cells
        = term->height_cells
        = term->width_px
        = term->height_px
        = -1;

#ifdef G_OS_WIN32
    {
        HANDLE chd = GetStdHandle (STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csb_info;

        if (chd != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo (chd, &csb_info))
        {
            term->width_cells = csb_info.srWindow.Right - csb_info.srWindow.Left + 1;
            term->height_cells = csb_info.srWindow.Bottom - csb_info.srWindow.Top + 1;
        }
    }
#elif defined(HAVE_SYS_IOCTL_H)
    {
        struct winsize w;
        gboolean have_winsz = FALSE;

        /* FIXME: Use tcgetwinsize() when it becomes more widely available.
         * See: https://www.austingroupbugs.net/view.php?id=1151#c3856 */

        if ((term->out_fd >= 0 && ioctl (term->out_fd, TIOCGWINSZ, &w) >= 0)
            || (term->err_fd >= 0 && ioctl (term->err_fd, TIOCGWINSZ, &w) >= 0)
            || (term->in_fd >= 0 && ioctl (term->in_fd, TIOCGWINSZ, &w) >= 0))
            have_winsz = TRUE;

# ifdef HAVE_CTERMID
        if (!have_winsz)
        {
            const gchar *term_path;
            gint fd = -1;

            term_path = ctermid (NULL);
            if (term_path)
                fd = g_open (term_path, O_RDONLY);

            if (fd >= 0)
            {
                if (ioctl (fd, TIOCGWINSZ, &w) >= 0)
                    have_winsz = TRUE;

                g_close (fd, NULL);
            }
        }
# endif /* HAVE_CTERMID */

        if (have_winsz)
        {
            term->width_cells = w.ws_col;
            term->height_cells = w.ws_row;
            term->width_px = w.ws_xpixel;
            term->height_px = w.ws_ypixel;
        }
    }
#endif /* HAVE_SYS_IOCTL_H */

    if (term->width_cells <= 0)
        term->width_cells = -1;
    if (term->height_cells <= 2)
        term->height_cells = -1;

    /* If .ws_xpixel and .ws_ypixel are filled out, we can calculate
     * aspect information for the font used. Sixel-capable terminals
     * like mlterm set these fields, but most others do not. */

    if (term->width_px > PIXEL_EXTENT_MAX
        || term->height_px > PIXEL_EXTENT_MAX)
    {
        /* https://github.com/hpjansson/chafa/issues/62 */
        term->width_px = -1;
        term->height_px = -1;
    }
    else if (term->width_px <= 0 || term->height_px <= 0)
    {
        term->width_px = -1;
        term->height_px = -1;
    }

    term->have_tty_size = TRUE;
}

/* ----------- *
 * Seq helpers *
 * ----------- */

static const ChafaTermSeq sixel_seqs [] =
{
    CHAFA_TERM_SEQ_BEGIN_SIXELS,
    CHAFA_TERM_SEQ_END_SIXELS,
    CHAFA_TERM_SEQ_ENABLE_SIXEL_SCROLLING,
    CHAFA_TERM_SEQ_DISABLE_SIXEL_SCROLLING,
    CHAFA_TERM_SEQ_SET_SIXEL_ADVANCE_DOWN,
    CHAFA_TERM_SEQ_SET_SIXEL_ADVANCE_RIGHT
};

static void
supplement_seq (ChafaTermInfo *dest, ChafaTermInfo *src, ChafaTermSeq seq)
{
    if (!chafa_term_info_have_seq (dest, seq))
        chafa_term_info_set_seq (dest,
                                 seq,
                                 chafa_term_info_get_seq (src, seq),
                                 NULL);
}

static void
supplement_seqs (ChafaTermInfo *dest, ChafaTermInfo *src,
                 const ChafaTermSeq *seqs, gint n_seqs)
{
    gint i;

    for (i = 0; i < n_seqs; i++)
        supplement_seq (dest, src, seqs [i]);
}

/* ----------------------- *
 * Internal event handling *
 * ----------------------- */

/* We peek at all incoming events and update state based on some of them
 * before they're passed on to the user.
 *
 * Events are handled before they're put on the event queue. */

static void
apply_probe_results (ChafaTerm *term)
{
    if (!term->probe_success)
        return;

    if (!term->default_term_info)
        term->default_term_info = chafa_term_db_get_fallback_info (chafa_term_db_get_default ());

    if (term->probe_found_sixel && !chafa_term_info_have_seq (term->term_info,
                                                              CHAFA_TERM_SEQ_BEGIN_SIXELS))
    {
        supplement_seqs (term->term_info, term->default_term_info,
                         sixel_seqs, G_N_ELEMENTS (sixel_seqs));
    }
}

static gboolean
handle_primary_da_event (ChafaTerm *term, ChafaEvent *event)
{
    gint arg;
    gint i;

    if (chafa_event_get_seq (event) != CHAFA_TERM_SEQ_PRIMARY_DEVICE_ATTRIBUTES)
        return FALSE;

    for (i = 0; (arg = chafa_event_get_seq_arg (event, i)) >= 0; i++)
    {
        switch (arg)
        {
            case 4:
                term->probe_found_sixel = TRUE;
                break;
            default:
                break;
        }
    }

    term->probe_success = TRUE;
    apply_probe_results (term);
    return TRUE;
}

static gboolean
handle_eof_event (ChafaTerm *term, ChafaEvent *event)
{
    if (chafa_event_get_type (event) != CHAFA_EOF_EVENT)
        return FALSE;

    term->in_eof_seen = TRUE;
    return TRUE;
}

typedef gboolean (*EventHandler)(ChafaTerm *, ChafaEvent *);

/* FIXME: Default FG and BG colors */
static const EventHandler event_handlers [] =
{
    handle_primary_da_event,
    handle_eof_event,
    NULL
};

static void
handle_event (ChafaTerm *term, ChafaEvent *event)
{
    gint i;

    if (!event)
        return;

    for (i = 0; event_handlers [i]; i++)
    {
        if (event_handlers [i] (term, event))
            break;
    }
}

/* ------------------ *
 * MS Windows helpers *
 * ------------------ */

#ifdef G_OS_WIN32
static gint win32_global_init_depth;

static UINT win32_saved_console_output_cp;
static UINT win32_saved_console_input_cp;

static gboolean win32_input_is_file;
static gboolean win32_output_is_file;

static void
win32_global_init (void)
{
    HANDLE chd;

    if (g_atomic_int_add (&win32_global_init_depth, 1) > 0)
        return;

    win32_saved_console_output_cp = GetConsoleOutputCP ();
    win32_saved_console_input_cp = GetConsoleCP ();

    /* Enable ANSI escape sequence parsing etc. on MS Windows command prompt */
    chd = GetStdHandle (STD_INPUT_HANDLE);
    if (chd != INVALID_HANDLE_VALUE)
    {
        DWORD bitmask = ENABLE_PROCESSED_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT;

        if (!SetConsoleMode (chd, bitmask))
        {
            if (GetLastError () == ERROR_INVALID_HANDLE)
            {
                win32_input_is_file = TRUE;
            }
        }
    }

    /* Enable ANSI escape sequence parsing etc. on MS Windows command prompt */
    chd = GetStdHandle (STD_OUTPUT_HANDLE);
    if (chd != INVALID_HANDLE_VALUE)
    {   
        DWORD bitmask = ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT;

#if 0
        if (!options.is_conhost_mode)
#endif
        bitmask |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;

        if (!SetConsoleMode (chd, bitmask))
        {
            if (GetLastError () == ERROR_INVALID_HANDLE)
            {
                win32_output_is_file = TRUE;
            }
            else
            {
                /* Compatibility with older MS Windows versions */
                SetConsoleMode (chd,ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT);
            }
        }   
    }

    /* Set UTF-8 code page output */
    SetConsoleOutputCP (65001);

    /* Set UTF-8 code page input, for good measure */
    SetConsoleCP (65001);
}

static void
win32_global_deinit (void)
{
    if (g_atomic_int_dec_and_test (&win32_global_init_depth))
        return;

    SetConsoleOutputCP (win32_saved_console_output_cp);
    SetConsoleCP (win32_saved_console_input_cp);
}

static void
win32_term_init (ChafaTerm *term)
{
    if (term->in_fd >= 0)
        setmode (term->in_fd, O_BINARY);
    setmode (term->out_fd, O_BINARY);
    win32_global_init ();
}

static void
win32_term_deinit (ChafaTerm *term)
{
    win32_global_deinit ();
}
#endif

/* ----------------------- *
 * Mid-level I/O machinery *
 * ----------------------- */

static ChafaEvent *
in_sync_pull (ChafaTerm *term, gint timeout_ms)
{
    ChafaEvent *event = NULL;
    gint64 end_time;

    if ((event = chafa_parser_pop_event (term->parser)))
        return event;

    if (timeout_ms > 0)
        end_time = g_get_monotonic_time () + timeout_ms * 1000;

    g_mutex_lock (&term->mutex);

    for (;;)
    {
        guchar buf [READ_BUF_MAX];
        gint len;

        /* FIXME: Can use peek here and save copying */
        len = chafa_byte_fifo_pop (term->in_fifo, buf, READ_BUF_MAX);
        chafa_parser_push_data (term->parser, buf, len);

        if ((event = chafa_parser_pop_event (term->parser)))
            break;

        if (timeout_ms > 0)
        {
            if (g_get_monotonic_time () >= end_time)
                break;

            g_cond_wait_until (&term->cond, &term->mutex, end_time);
        }
        else
        {
            g_cond_wait (&term->cond, &term->mutex);
        }
    }

    g_mutex_unlock (&term->mutex);
    return event;
}

static gboolean
in_idle_func (gpointer data)
{
    ChafaTerm *term = data;

    /* Dispatch read events in main thread */

    g_mutex_lock (&term->mutex);
    term->in_idle_id = 0;

    for (;;)
    {
        guchar buf [READ_BUF_MAX];
        gint len;

        /* FIXME: Can use peek here and save copying */
        len = chafa_byte_fifo_pop (term->in_fifo, buf, READ_BUF_MAX);
        if (len < 1)
            break;

        chafa_parser_push_data (term->parser, buf, len);
    }

    g_mutex_unlock (&term->mutex);

    /* TODO: Actually dispatch the events from parser */

    return G_SOURCE_REMOVE;
}

static gpointer
in_thread_main (gpointer data)
{
    ChafaTerm *term = data;

    for (;;)
    {
        guchar buf [READ_BUF_MAX];
        gint len;

        len = read_from_stdin (term, buf, READ_BUF_MAX);

        g_mutex_lock (&term->mutex);

        if (len < 0 || term->shutdown_reqd)
        {
            break;
        }
        else if (len > 0)
        {
            chafa_byte_fifo_push (term->in_fifo, buf, len);
            g_cond_broadcast (&term->cond);
            if (!term->in_idle_id)
                term->in_idle_id = g_idle_add (in_idle_func, term);
        }

        while (chafa_byte_fifo_get_len (term->in_fifo) > IN_FIFO_DEFAULT_MAX
               && !term->shutdown_reqd)
            g_cond_wait (&term->cond, &term->mutex);

        if (term->shutdown_reqd)
            break;

        g_mutex_unlock (&term->mutex);
    }

    term->in_shutdown_done = TRUE;
    g_cond_broadcast (&term->cond);
    g_mutex_unlock (&term->mutex);
    return NULL;
}

static gpointer
out_thread_main (gpointer data)
{
    ChafaTerm *term = data;
    gboolean io_error = FALSE;

    for (;;)
    {
        guchar buf [WRITE_BUF_MAX];
        gint len;
        gboolean to_err = FALSE;

        g_mutex_lock (&term->mutex);

        if (io_error || term->shutdown_reqd)
            break;

        if (!chafa_byte_fifo_get_len (term->out_fifo)
            && !chafa_byte_fifo_get_len (term->err_fifo))
        {
            /* Pending output has now left the process. Signal main thread; it
             * may be waiting to finish a flush */
            term->out_drained = TRUE;
            g_cond_broadcast (&term->cond);
        }

        len = 0;

        while (!term->shutdown_reqd)
        {
            len = chafa_byte_fifo_pop (term->err_fifo, buf, WRITE_BUF_MAX);
            if (len)
            {
                to_err = TRUE;
                break;
            }

            len = chafa_byte_fifo_pop (term->out_fifo, buf, WRITE_BUF_MAX);
            if (len)
            {
                break;
            }

            g_cond_wait (&term->cond, &term->mutex);
        }

        g_mutex_unlock (&term->mutex);

        if (to_err)
        {
            if (!write_to_stderr (term, buf, len))
                io_error = TRUE;
        }
        else
        {
            if (!write_to_stdout (term, buf, len))
                io_error = TRUE;
        }
    }

    term->out_shutdown_done = TRUE;
    g_cond_broadcast (&term->cond);
    g_mutex_unlock (&term->mutex);
    return NULL;
}

/* --------------------- *
 * Construct and destroy *
 * --------------------- */

static ChafaTerm *
new_default (void)
{
    ChafaTerm *term;
    gchar **envp;

    envp = g_get_environ ();

    term = g_new0 (ChafaTerm, 1);

    term->term_info = chafa_term_db_detect (chafa_term_db_get_default (), envp);
    term->parser = chafa_parser_new (term->term_info);

    term->in_buf_max = IN_FIFO_DEFAULT_MAX;
    term->out_buf_max = OUT_FIFO_DEFAULT_MAX;

    term->in_fifo = chafa_byte_fifo_new ();
    term->out_fifo = chafa_byte_fifo_new ();
    term->err_fifo = chafa_byte_fifo_new ();

    term->wakeup = chafa_wakeup_new ();
    term->event_queue = g_queue_new ();

    term->in_fd = fileno (stdin);
    term->out_fd = fileno (stdout);
    term->err_fd = fileno (stderr);

    if (!isatty (term->in_fd))
        term->in_fd = -1;

#ifdef G_OS_WIN32
    win32_term_init (term);
#else
    if (term->in_fd >= 0)
        g_unix_set_fd_nonblocking (term->in_fd, TRUE, NULL);
    g_unix_set_fd_nonblocking (term->out_fd, FALSE, NULL);
    g_unix_set_fd_nonblocking (term->err_fd, FALSE, NULL);
#endif

    if (term->in_fd >= 0 && term->out_fd >= 0
        && isatty (term->in_fd) && isatty (term->out_fd))
        term->interactive_supported = TRUE;

    get_tty_size (term);

    g_mutex_init (&term->mutex);
    g_cond_init (&term->cond);
    term->in_thread = g_thread_new ("term-in", in_thread_main, term);
    term->out_thread = g_thread_new ("term-out", out_thread_main, term);

    g_strfreev (envp);
    return term;
}

static ChafaTerm *
instantiate_singleton (G_GNUC_UNUSED gpointer data)
{
    return new_default ();
}

/* ---------- *
 * Public API *
 * ---------- */

void
chafa_term_destroy (ChafaTerm *term)
{
    g_return_if_fail (term != NULL);

    g_mutex_lock (&term->mutex);

    term->shutdown_reqd = TRUE;
    chafa_wakeup_signal (term->wakeup);
    g_cond_broadcast (&term->cond);

    while (!term->in_shutdown_done || !term->out_shutdown_done)
        g_cond_wait (&term->cond, &term->mutex);

    g_mutex_unlock (&term->mutex);

    g_thread_join (term->in_thread);
    g_thread_join (term->out_thread);

    g_mutex_clear (&term->mutex);
    g_cond_clear (&term->cond);
    chafa_wakeup_free (term->wakeup);
    g_queue_free_full (term->event_queue, g_free);

    chafa_byte_fifo_destroy (term->in_fifo);
    chafa_byte_fifo_destroy (term->out_fifo);
    chafa_byte_fifo_destroy (term->err_fifo);

    chafa_term_info_unref (term->term_info);
    if (term->default_term_info)
        chafa_term_info_unref (term->default_term_info);

    g_free (term);
}

ChafaTerm *
chafa_term_get_default (void)
{
  static GOnce my_once = G_ONCE_INIT;

  g_once (&my_once, (GThreadFunc) instantiate_singleton, NULL);
  return my_once.retval;
}

gint
chafa_term_get_buffer_max (ChafaTerm *term)
{
    return term->out_buf_max;
}

void
chafa_term_set_buffer_max (ChafaTerm *term, gint max)
{
    term->out_buf_max = max < 1 ? -1 : max;
}

ChafaTermInfo *
chafa_term_get_term_info (ChafaTerm *term)
{
    return term->term_info;
}

ChafaEvent *
chafa_term_read_event (ChafaTerm *term, guint timeout_ms)
{
    ChafaEvent *event = NULL;
    gint64 start_time;
    gint remain_ms = timeout_ms;

    if (term->in_fd < 0)
        return NULL;

    event = g_queue_pop_tail (term->event_queue);
    if (event)
        goto out;

    if (term->in_eof_seen)
        goto out;

    if (timeout_ms > 0)
        start_time = g_get_monotonic_time ();

    while (!(event = in_sync_pull (term, timeout_ms > 0 ? remain_ms : -1)))
    {
        if (timeout_ms > 0)
        {
            remain_ms -= (g_get_monotonic_time () - start_time) * 1000;
            if (remain_ms <= 0)
                break;
        }
    }

    handle_event (term, event);

out:
    return event;
}

void
chafa_term_write (ChafaTerm *term, gconstpointer data, gsize len)
{
    /* FIXME: Apply emergency brakes if buffer limit reached */

    g_mutex_lock (&term->mutex);
    term->out_drained = FALSE;
    chafa_byte_fifo_push (term->out_fifo, data, len);
    g_cond_broadcast (&term->cond);
    g_mutex_unlock (&term->mutex);
}

gint
chafa_term_print (ChafaTerm *term, const gchar *format, ...)
{
    gchar *str = NULL;
    va_list args;
    gint result;

    va_start (args, format);
    result = g_vasprintf (&str, format, args);
    va_end (args);

    if (result > 0)
        chafa_term_write (term, str, result);

    g_free (str);
    return result;
}

void
chafa_term_print_seq (ChafaTerm *term, ChafaTermSeq seq, ...)
{
    va_list args;
    gchar *str;

    va_start (args, seq);
    str = chafa_term_info_emit_seq_valist (term->term_info, seq, &args);
    va_end (args);

    if (str)
        chafa_term_write (term, str, strlen (str));

    g_free (str);
}

gboolean
chafa_term_flush (ChafaTerm *term)
{
    g_mutex_lock (&term->mutex);
    while (!term->out_drained)
        g_cond_wait (&term->cond, &term->mutex);
    g_mutex_unlock (&term->mutex);

    return TRUE;
}

void
chafa_term_write_err (ChafaTerm *term, gconstpointer data, gsize len)
{
    g_mutex_lock (&term->mutex);
    term->out_drained = FALSE;
    chafa_byte_fifo_push (term->err_fifo, data, len);
    g_cond_broadcast (&term->cond);
    g_mutex_unlock (&term->mutex);
}

gint
chafa_term_print_err (ChafaTerm *term, const gchar *format, ...)
{
    gchar *str = NULL;
    va_list args;
    gint result;

    va_start (args, format);
    result = g_vasprintf (&str, format, args);
    va_end (args);

    if (result > 0)
        chafa_term_write_err (term, str, result);

    g_free (str);
    return result;
}

void
chafa_term_get_size_px (ChafaTerm *term, gint *width_px_out, gint *height_px_out)
{
    if (!term->have_tty_size)
        get_tty_size (term);

    if (width_px_out)
        *width_px_out = term->width_px;
    if (height_px_out)
        *height_px_out = term->height_px;
}

void
chafa_term_get_size_cells (ChafaTerm *term, gint *width_cells_out, gint *height_cells_out)
{
    if (!term->have_tty_size)
        get_tty_size (term);

    if (width_cells_out)
        *width_cells_out = term->width_cells;
    if (height_cells_out)
        *height_cells_out = term->height_cells;
}

gboolean
chafa_term_sync_probe (ChafaTerm *term, gint timeout_ms)
{
    ChafaEvent *event;
    gint64 start_time;
    gint remain_ms = timeout_ms;
#ifdef HAVE_TERMIOS_H
    struct termios saved_termios;
    gboolean termios_changed = FALSE;
#endif

    if (term->probe_success)
        return TRUE;
    if (!term->interactive_supported)
        return FALSE;

    if (timeout_ms > 0)
        start_time = g_get_monotonic_time ();

    /* Terminal must be in raw mode for response to get picked up without
     * user interaction. */
#ifdef HAVE_TERMIOS_H
    ensure_raw_mode_enabled (term, &saved_termios, &termios_changed);
#endif

    chafa_term_print_seq (term, CHAFA_TERM_SEQ_QUERY_PRIMARY_DEVICE_ATTRIBUTES, -1);
    term->probe_attempt = TRUE;

    while ((event = in_sync_pull (term, timeout_ms > 0 ? remain_ms : -1)))
    {
        g_queue_push_head (term->event_queue, event);
        handle_event (term, event);

        if (term->probe_success || term->in_eof_seen)
            break;

        if (timeout_ms > 0)
        {
            remain_ms -= (g_get_monotonic_time () - start_time) * 1000;
            if (remain_ms <= 0)
                break;
        }
    }

#ifdef HAVE_TERMIOS_H
    restore_termios (term, &saved_termios, &termios_changed);
#endif

    return term->probe_success;
}

void
chafa_term_notify_size_changed (ChafaTerm *term)
{
    get_tty_size (term);
}
