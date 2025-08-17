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

#include <stdio.h>  /* ctermid */
#include <sys/types.h>  /* open */
#include <fcntl.h>  /* open */
#include <unistd.h>  /* STDOUT_FILENO */
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>  /* ioctl */
#endif
#ifdef HAVE_TERMIOS_H
# include <termios.h>  /* tcgetattr, tcsetattr */
#endif

#include "chafa.h"
#include "chafa-byte-fifo.h"
#include "chafa-stream-reader.h"
#include "chafa-stream-writer.h"
#include "chafa-term.h"

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

/* Maximum width or height of the terminal, in pixels. If it claims to be
 * bigger than this, assume it's broken. */
#define PIXEL_EXTENT_MAX (8192 * 3)
#define CELL_EXTENT_PX_MAX 8192

/* Stack buffer size */
#define READ_BUF_MAX 4096

struct ChafaTerm
{
    ChafaTermInfo *term_info;
    ChafaTermInfo *default_term_info;
    ChafaParser *parser;
    ChafaStreamReader *reader;
    ChafaStreamWriter *writer;
    ChafaStreamWriter *err_writer;

    gint width_cells, height_cells;
    gint width_px, height_px;
    gint cell_width_px, cell_height_px;

    /* Default FG/BG colors. Byte order is XRGB native. -1 if unknown */
    gint32 default_fg_rgb;
    gint32 default_bg_rgb;

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

    GQueue *event_queue;
    guint in_idle_id;
};

/* ------------------ *
 * MS Windows helpers *
 * ------------------ */

#ifdef G_OS_WIN32

static UINT win32_saved_console_output_cp;
static UINT win32_saved_console_input_cp;
static gint win32_global_init_depth;

static void
win32_global_init (void)
{
    if (g_atomic_int_add (&win32_global_init_depth, 1) > 0)
        return;

    win32_saved_console_output_cp = GetConsoleOutputCP ();
    win32_saved_console_input_cp = GetConsoleCP ();

    /* Set UTF-8 code page output */
    SetConsoleOutputCP (65001);

    /* Set UTF-8 code page input, for good measure */
    SetConsoleCP (65001);
}

static void
win32_global_deinit (void)
{
    if (!g_atomic_int_dec_and_test (&win32_global_init_depth))
        return;

    SetConsoleOutputCP (win32_saved_console_output_cp);
    SetConsoleCP (win32_saved_console_input_cp);
}

static void
win32_term_init (ChafaTerm *term)
{
    win32_global_init ();
}

static void
win32_term_deinit (ChafaTerm *term)
{
    win32_global_deinit ();
}

static HANDLE
win32_get_reader_handle (ChafaTerm *term)
{
    if (term->reader)
    {
        gint fd = chafa_stream_reader_get_fd (term->reader);
        if (fd >= 0)
            return (HANDLE) _get_osfhandle (fd);
    }

    return INVALID_HANDLE_VALUE;
}

static HANDLE
win32_get_writer_handle (ChafaTerm *term)
{
    if (term->reader)
    {
        gint fd = chafa_stream_reader_get_fd (term->reader);
        if (fd >= 0)
            return (HANDLE) _get_osfhandle (fd);
    }

    return INVALID_HANDLE_VALUE;
}

#endif

/* -------------------------------- *
 * Low-level I/O and tty whispering *
 * -------------------------------- */

#ifdef HAVE_TERMIOS_H
static void
ensure_raw_mode_enabled (ChafaTerm *term, struct termios *saved_termios,
                         gboolean *termios_changed)
{
    struct termios t;

    if (!term->reader)
        return;

    tcgetattr (chafa_stream_reader_get_fd (term->reader), saved_termios);

    t = *saved_termios;
    t.c_lflag &= ~(ECHO | ICANON);

    if (t.c_lflag != saved_termios->c_lflag)
    {
        *termios_changed = TRUE;
        tcsetattr (chafa_stream_reader_get_fd (term->reader), TCSANOW, &t);
    }
    else
    {
        *termios_changed = FALSE;
    }
}

static void
restore_termios (ChafaTerm *term, const struct termios *saved_termios, gboolean *termios_changed)
{
    if (!term->reader || !*termios_changed)
        return;

    tcsetattr (chafa_stream_reader_get_fd (term->reader), TCSANOW, saved_termios);
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
        HANDLE chd = win32_get_writer_handle (term);
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
        gint in_fd = -1, out_fd = -1, err_fd = -1;

        if (term->reader)
            in_fd = chafa_stream_reader_get_fd (term->reader);
        if (term->writer)
            out_fd = chafa_stream_writer_get_fd (term->writer);
        if (term->err_writer)
            err_fd = chafa_stream_writer_get_fd (term->err_writer);

        /* FIXME: Use tcgetwinsize() when it becomes more widely available.
         * See: https://www.austingroupbugs.net/view.php?id=1151#c3856 */

        if ((out_fd >= 0 && ioctl (out_fd, TIOCGWINSZ, &w) >= 0)
            || (err_fd >= 0 && ioctl (err_fd, TIOCGWINSZ, &w) >= 0)
            || (in_fd >= 0 && ioctl (in_fd, TIOCGWINSZ, &w) >= 0))
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

    if (term->width_cells > 0 && term->height_cells > 0
        && term->width_px > 0 && term->height_px > 0)
    {
        term->cell_width_px = term->width_px / term->width_cells;
        term->cell_height_px = term->height_px / term->height_cells;
    }
}

static gint
probe_color_to_packed_rgb (const gint *c)
{
    return ((c [0] / 256) << 16)
        | ((c [1] / 256) << 8)
        | (c [2] / 256);
}

static gboolean
handle_default_fg_event (ChafaTerm *term, ChafaEvent *event)
{
    gint c [3];
    gint i;

    if (chafa_event_get_seq (event) != CHAFA_TERM_SEQ_SET_DEFAULT_FG)
        return FALSE;

    for (i = 0; i < 3; i++)
        c [i] = chafa_event_get_seq_arg (event, i);

    term->default_fg_rgb = probe_color_to_packed_rgb (c);
    return TRUE;
}

static gboolean
handle_default_bg_event (ChafaTerm *term, ChafaEvent *event)
{
    gint c [3];
    gint i;

    if (chafa_event_get_seq (event) != CHAFA_TERM_SEQ_SET_DEFAULT_BG)
        return FALSE;

    for (i = 0; i < 3; i++)
        c [i] = chafa_event_get_seq_arg (event, i);

    term->default_bg_rgb = probe_color_to_packed_rgb (c);
    return TRUE;
}

static gboolean
handle_text_area_size_cells_event (ChafaTerm *term, ChafaEvent *event)
{
    gint c [2];
    gint i;

    if (chafa_event_get_seq (event) != CHAFA_TERM_SEQ_TEXT_AREA_SIZE_CELLS)
        return FALSE;

    for (i = 0; i < 2; i++)
        c [i] = chafa_event_get_seq_arg (event, i);

    if (c [0] > 0 && c [1] > 0)
    {
        term->width_cells = c [1];
        term->height_cells = c [0];
    }

    return TRUE;
}

static gboolean
handle_text_area_size_px_event (ChafaTerm *term, ChafaEvent *event)
{
    gint c [2];
    gint i;

    if (chafa_event_get_seq (event) != CHAFA_TERM_SEQ_TEXT_AREA_SIZE_PX)
        return FALSE;

    for (i = 0; i < 2; i++)
        c [i] = chafa_event_get_seq_arg (event, i);

    if (c [0] > 0 && c [0] < PIXEL_EXTENT_MAX
        && c [1] > 0 && c [1] < PIXEL_EXTENT_MAX)
    {
        term->width_px = c [1];
        term->height_px = c [0];
    }

    return TRUE;
}

static gboolean
handle_cell_size_px_event (ChafaTerm *term, ChafaEvent *event)
{
    gint c [2];
    gint i;

    if (chafa_event_get_seq (event) != CHAFA_TERM_SEQ_CELL_SIZE_PX)
        return FALSE;

    for (i = 0; i < 2; i++)
        c [i] = chafa_event_get_seq_arg (event, i);

    if (c [0] > 0 && c [0] < CELL_EXTENT_PX_MAX
        && c [1] > 0 && c [1] < CELL_EXTENT_PX_MAX)
    {
        term->cell_width_px = c [1];
        term->cell_height_px = c [0];
    }

    return TRUE;
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

typedef gboolean (*EventHandler) (ChafaTerm *, ChafaEvent *);

static const EventHandler event_handlers [] =
{
    handle_default_fg_event,
    handle_default_bg_event,
    handle_text_area_size_cells_event,
    handle_text_area_size_px_event,
    handle_cell_size_px_event,
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

/* ----------------------- *
 * Mid-level I/O machinery *
 * ----------------------- */

static ChafaEvent *
in_sync_pull (ChafaTerm *term, gint timeout_ms)
{
    ChafaEvent *event = NULL;
    gint64 end_time_us = -1;

    if ((event = chafa_parser_pop_event (term->parser)))
        return event;

    if (timeout_ms > 0)
        end_time_us = g_get_monotonic_time () + timeout_ms * 1000;

    for (;;)
    {
        guchar buf [READ_BUF_MAX];
        gint len;

        /* TODO: Break on EOF */

        len = chafa_stream_reader_read (term->reader, buf, READ_BUF_MAX);
        chafa_parser_push_data (term->parser, buf, len);

        if ((event = chafa_parser_pop_event (term->parser)))
            break;

        if (timeout_ms <= 0)
        {
            /* Wait indefinitely */
            chafa_stream_reader_wait (term->reader, -1);
        }
        else
        {
            /* Bail out if end time passed */
            if (!chafa_stream_reader_wait_until (term->reader, end_time_us))
                break;
        }
    }

    return event;
}

/* --------------------- *
 * Construct and destroy *
 * --------------------- */

static gboolean
fd_is_valid (gint fd)
{
    gboolean result;

    if (fd < 0)
        return FALSE;

#ifdef G_OS_WIN32
    result = ((HANDLE) _get_osfhandle (fd) != INVALID_HANDLE_VALUE) ? TRUE : FALSE;
#else
    result = (fcntl (fd, F_GETFL) >= 0) ? TRUE : FALSE;
#endif

    return result;
}

static ChafaTerm *
term_new_full (ChafaTermInfo *term_info, gint in_fd, gint out_fd, gint err_fd)
{
    ChafaTerm *term;

    term = g_new0 (ChafaTerm, 1);

    if (term_info)
    {
        chafa_term_info_ref (term_info);
        term->term_info = term_info;
    }
    else
    {
        gchar **envp;

        envp = g_get_environ ();
        term->term_info = chafa_term_db_detect (chafa_term_db_get_default (), envp);
        g_strfreev (envp);
    }

    term->parser = chafa_parser_new (term->term_info);

    term->width_cells = -1;
    term->height_cells = -1;
    term->width_px = -1;
    term->height_px = -1;
    term->cell_width_px = -1;
    term->cell_height_px = -1;

    term->default_fg_rgb = -1;
    term->default_bg_rgb = -1;

    term->event_queue = g_queue_new ();

    /* Verify that the fds are open before we do anything else with them. The
     * default terminal uses stdio (0, 1, 2), but these may have been closed by
     * the calling process. The fds may be reused later, e.g. when the
     * application opens a file. */

    if (fd_is_valid (in_fd))
        term->reader = chafa_stream_reader_new_from_fd (in_fd);
    if (fd_is_valid (out_fd))
        term->writer = chafa_stream_writer_new_from_fd (out_fd);
    if (fd_is_valid (err_fd))
        term->err_writer = chafa_stream_writer_new_from_fd (err_fd);

#ifdef G_OS_WIN32
    win32_term_init (term);
#endif

    if (term->reader && chafa_stream_reader_is_console (term->reader)
        && term->writer && chafa_stream_writer_is_console (term->writer))
        term->interactive_supported = TRUE;

    get_tty_size (term);
    return term;
}

static ChafaTerm *
term_new_default (void)
{
    return term_new_full (NULL,
                          STDIN_FILENO,
                          STDOUT_FILENO,
                          STDERR_FILENO);
}

static ChafaTerm *
instantiate_singleton (G_GNUC_UNUSED gpointer data)
{
    return term_new_default ();
}

/* ---------- *
 * Public API *
 * ---------- */

ChafaTerm *
chafa_term_new (ChafaTermInfo *term_info, gint in_fd, gint out_fd, gint err_fd)
{
    return term_new_full (term_info, in_fd, out_fd, err_fd);
}

void
chafa_term_destroy (ChafaTerm *term)
{
    g_return_if_fail (term != NULL);

    if (term->reader)
        chafa_stream_reader_unref (term->reader);
    if (term->writer)
        chafa_stream_writer_unref (term->writer);
    if (term->err_writer)
        chafa_stream_writer_unref (term->err_writer);

    g_queue_free_full (term->event_queue, g_free);

    chafa_term_info_unref (term->term_info);
    if (term->default_term_info)
        chafa_term_info_unref (term->default_term_info);

#ifdef G_OS_WIN32
    win32_term_deinit (term);
#endif

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
    if (term->writer)
        return chafa_stream_writer_get_buffer_max (term->writer);
    return -1;
}

void
chafa_term_set_buffer_max (ChafaTerm *term, gint buf_max)
{
    if (term->writer)
        chafa_stream_writer_set_buffer_max (term->writer, buf_max);
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

    if (!term->reader)
        goto out;

    event = g_queue_pop_tail (term->event_queue);
    if (event)
        goto out;

    if (term->in_eof_seen)
        goto out;

    event = in_sync_pull (term, timeout_ms);
    handle_event (term, event);

out:
    return event;
}

void
chafa_term_write (ChafaTerm *term, gconstpointer data, gint len)
{
    if (!term->writer)
        return;

    chafa_stream_writer_write (term->writer, data, len);
}

gint
chafa_term_print (ChafaTerm *term, const gchar *format, ...)
{
    gchar *str = NULL;
    va_list args;
    gint len = -1;

    if (!term->writer)
        return -1;

    va_start (args, format);
    len = g_vasprintf (&str, format, args);
    va_end (args);

    if (len > 0)
        chafa_stream_writer_write (term->writer, str, len);

    g_free (str);
    return len;
}

gint
chafa_term_print_seq (ChafaTerm *term, ChafaTermSeq seq, ...)
{
    va_list args;
    gchar *str;
    gint len = -1;

    if (!term->writer)
        return -1;

    va_start (args, seq);
    str = chafa_term_info_emit_seq_valist (term->term_info, seq, &args);
    va_end (args);

    if (str)
    {
        len = strlen (str);
        chafa_stream_writer_write (term->writer, str, len);
    }

    g_free (str);
    return len;
}

gboolean
chafa_term_flush (ChafaTerm *term)
{
    if (!term->writer)
        return FALSE;

    return chafa_stream_writer_flush (term->writer);
}

void
chafa_term_write_err (ChafaTerm *term, gconstpointer data, gint len)
{
    if (!term->err_writer)
        return;

    chafa_stream_writer_write (term->err_writer, data, len);
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

    chafa_term_print_seq (term, CHAFA_TERM_SEQ_QUERY_DEFAULT_FG, -1);
    chafa_term_print_seq (term, CHAFA_TERM_SEQ_QUERY_DEFAULT_BG, -1);
    chafa_term_print_seq (term, CHAFA_TERM_SEQ_QUERY_TEXT_AREA_SIZE_CELLS, -1);
    chafa_term_print_seq (term, CHAFA_TERM_SEQ_QUERY_TEXT_AREA_SIZE_PX, -1);
    chafa_term_print_seq (term, CHAFA_TERM_SEQ_QUERY_CELL_SIZE_PX, -1);
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
            remain_ms -= (g_get_monotonic_time () - start_time) / 1000;
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

gint32
chafa_term_get_default_fg_color (ChafaTerm *term)
{
    return term->default_fg_rgb;
}

gint32
chafa_term_get_default_bg_color (ChafaTerm *term)
{
    return term->default_bg_rgb;
}
