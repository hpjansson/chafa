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
#endif

/* Maximum width or height of the terminal, in pixels. If it claims to be
 * bigger than this, assume it's broken. */
#define PIXEL_EXTENT_MAX (8192 * 3)

struct ChafaTerm
{
    ChafaTermInfo *term_info;

    ChafaByteFifo *in_fifo, *out_fifo, *err_fifo;
    gint in_fd, out_fd, err_fd;

    gint width_cells, height_cells;
    gint width_px, height_px;
};

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
}

#ifdef G_OS_WIN32
static gint win32_global_init_depth;

static UINT win32_saved_console_output_cp;
static UINT win32_saved_console_input_cp;

static gboolean win32_output_is_file;

static void
win32_global_init (void)
{
    HANDLE chd;

    if (g_atomic_int_add (&win32_global_init_depth, 1) > 0)
        return;

    chd = GetStdHandle (STD_OUTPUT_HANDLE);
    win32_saved_console_output_cp = GetConsoleOutputCP ();
    win32_saved_console_input_cp = GetConsoleCP ();

    /* Enable ANSI escape sequence parsing etc. on MS Windows command prompt */
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

static ChafaTerm *
new_default (void)
{
    ChafaTerm *term;

    term = g_new0 (ChafaTerm, 1);

    term->in_fifo = chafa_byte_fifo_new ();
    term->out_fifo = chafa_byte_fifo_new ();
    term->err_fifo = chafa_byte_fifo_new ();

    term->in_fd = fileno (stdin);
    term->out_fd = fileno (stdout);
    term->err_fd = fileno (stderr);

#ifdef G_OS_WIN32
    win32_term_init (term);
#endif

    get_tty_size (term);
    return term;
}

static ChafaTerm *
instantiate_singleton (G_GNUC_UNUSED gpointer data)
{
    return new_default ();
}

ChafaTerm *
chafa_term_get_default (void)
{
  static GOnce my_once = G_ONCE_INIT;

  g_once (&my_once, (GThreadFunc) instantiate_singleton, NULL);
  return my_once.retval;
}

void
chafa_term_destroy (ChafaTerm *term)
{
    g_return_if_fail (term != NULL);

    chafa_byte_fifo_destroy (term->in_fifo);
    chafa_byte_fifo_destroy (term->out_fifo);
    chafa_byte_fifo_destroy (term->err_fifo);
    g_free (term);
}

ChafaTermInfo *
chafa_term_get_term_info (ChafaTerm *term)
{
    return term->term_info;
}

ChafaEvent *
chafa_term_read_event (ChafaTerm *term, guint timeout_ms)
{
}

void
chafa_term_write (ChafaTerm *term, gconstpointer data, gsize len)
{
    chafa_byte_fifo_push (term->out_fifo, data, len);
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

static gint
flush_fifo (ChafaTerm *term, ChafaByteFifo *fifo)
{
    gint total_len = 0;

    for (;;)
    {
        gconstpointer data;
        gint len;

        data = chafa_byte_fifo_peek (fifo, &len);
        if (!data)
            break;

        if (!write_to_stdout (term, data, len))
            break;

        chafa_byte_fifo_drop (fifo, len);
        total_len += len;
    }

    return total_len;
}

gint
chafa_term_flush (ChafaTerm *term)
{
    gint len, total_len = 0;

    len = flush_fifo (term, term->err_fifo);
    if (chafa_byte_fifo_get_len (term->err_fifo) > 0)
    {
        /* FIXME: Close */
        term->err_fd = -1;
    }

    if (len > 0)
        total_len += len;

    len = flush_fifo (term, term->out_fifo);
    if (chafa_byte_fifo_get_len (term->out_fifo) > 0)
    {
        /* FIXME: Close */
        term->out_fd = -1;
    }

    if (len > 0)
        total_len += len;

    return total_len;
}

void
chafa_term_write_err (ChafaTerm *term, gconstpointer data, gsize len)
{
    chafa_byte_fifo_push (term->err_fifo, data, len);
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
    if (width_px_out)
        *width_px_out = term->width_px;
    if (height_px_out)
        *height_px_out = term->height_px;
}

void
chafa_term_get_size_cells (ChafaTerm *term, gint *width_cells_out, gint *height_cells_out)
{
    if (width_cells_out)
        *width_cells_out = term->width_cells;
    if (height_cells_out)
        *height_cells_out = term->height_cells;
}

void
chafa_term_sync_probe (ChafaTerm *term, guint timeout_ms)
{
}

void
chafa_term_notify_size_changed (ChafaTerm *term)
{
    get_tty_size (term);
}
