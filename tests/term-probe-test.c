/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2026 Hans Petter Jansson
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

#include <chafa.h>
#include "internal/chafa-private.h"

#if !defined (G_OS_WIN32) && defined (HAVE_TERMIOS_H)

#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PROBE_TIMEOUT_MS 3000
#define RESPONDER_TIMEOUT_US (10 * G_USEC_PER_SEC)

#define DA1_QUERY "\033[0c"
#define DA1_REPLY "\033[?62;22c"
#define XTVERSION_QUERY "\033[>0q"
#define KITTY_QUERY_PREFIX "\033_G"
#define KITTY_OK_REPLY "\033_Gi=31;OK\033\\"
#define KITTY_ERROR_REPLY "\033_Gi=31;ENOTSUPPORTED:no graphics\033\\"

/* Plays the part of a terminal on the master side of a pty. Each DA1 query
 * is answered after the queries that precede it, the way terminals process
 * input in order. */
typedef struct
{
    gint master_fd;
    const gchar *version_reply;
    const gchar *kitty_reply;

    gboolean saw_version_query;
    gboolean saw_kitty_query;
}
Responder;

static void
write_all (gint fd, const gchar *str)
{
    gsize len = strlen (str);

    while (len > 0)
    {
        gssize n = write (fd, str, len);

        g_assert (n > 0);
        str += n;
        len -= n;
    }
}

static gpointer
responder_thread (gpointer data)
{
    Responder *r = data;
    GString *buf = g_string_new ("");
    gint64 end_time = g_get_monotonic_time () + RESPONDER_TIMEOUT_US;
    gint n_da1 = 0;

    /* Keeps going after the first DA1, so a Kitty query that shouldn't have
     * been sent is still seen */
    while (n_da1 < 2 && g_get_monotonic_time () < end_time)
    {
        struct pollfd pfd = { r->master_fd, POLLIN, 0 };
        gchar rbuf [1024];
        gchar *da1;
        gssize n;

        if (poll (&pfd, 1, 100) <= 0)
            continue;

        n = read (r->master_fd, rbuf, sizeof (rbuf));
        if (n <= 0)
            break;
        g_string_append_len (buf, rbuf, n);

        while ((da1 = strstr (buf->str, DA1_QUERY)))
        {
            gchar *before = g_strndup (buf->str, da1 - buf->str);

            if (strstr (before, XTVERSION_QUERY))
            {
                r->saw_version_query = TRUE;
                if (r->version_reply)
                    write_all (r->master_fd, r->version_reply);
            }

            if (strstr (before, KITTY_QUERY_PREFIX))
            {
                r->saw_kitty_query = TRUE;
                if (r->kitty_reply)
                    write_all (r->master_fd, r->kitty_reply);
            }

            write_all (r->master_fd, DA1_REPLY);
            g_string_erase (buf, 0, da1 - buf->str + strlen (DA1_QUERY));
            g_free (before);
            n_da1++;
        }
    }

    g_string_free (buf, TRUE);
    return NULL;
}

/* Returns the best pixel mode after probing. If @n_input_out is non-NULL,
 * it's set to the number of unichar events left for the application. */
static ChafaPixelMode
run_probe (const gchar *term_env, Responder *r, gint *n_input_out)
{
    gchar *envp [] = { (gchar *) term_env, NULL };
    ChafaTermInfo *ti;
    ChafaTerm *term;
    ChafaPixelMode pixel_mode;
    GThread *thread;
    gint master_fd, slave_fd;

    master_fd = posix_openpt (O_RDWR | O_NOCTTY);
    g_assert_cmpint (master_fd, >=, 0);
    g_assert_cmpint (grantpt (master_fd), ==, 0);
    g_assert_cmpint (unlockpt (master_fd), ==, 0);
    slave_fd = open (ptsname (master_fd), O_RDWR | O_NOCTTY);
    g_assert_cmpint (slave_fd, >=, 0);

    ti = chafa_term_db_detect (chafa_term_db_get_default (), envp);
    term = chafa_term_new (ti, slave_fd, slave_fd, -1);
    chafa_term_info_unref (ti);

    r->master_fd = master_fd;
    thread = g_thread_new ("responder", responder_thread, r);

    g_assert_true (chafa_term_sync_probe (term, PROBE_TIMEOUT_MS));
    pixel_mode = chafa_term_info_get_best_pixel_mode (chafa_term_get_term_info (term));

    if (n_input_out)
    {
        ChafaEvent *event;

        *n_input_out = 0;
        while ((event = chafa_term_read_event (term, 1)))
        {
            if (chafa_event_get_type (event) == CHAFA_UNICHAR_EVENT)
                (*n_input_out)++;
            g_free (event);
        }
    }

    /* The responder stops after its last DA1 or when the master side
     * hangs up */
    chafa_term_destroy (term);
    close (slave_fd);
    g_thread_join (thread);
    close (master_fd);

    return pixel_mode;
}

static void
unknown_supported_test (void)
{
    Responder r = { 0 };

    r.version_reply = "\033P>|newterm 1.0\033\\";
    r.kitty_reply = KITTY_OK_REPLY;

    g_assert_cmpint (run_probe ("TERM=xterm-256color", &r, NULL), ==, CHAFA_PIXEL_MODE_KITTY);
    g_assert_true (r.saw_version_query);
    g_assert_true (r.saw_kitty_query);
}

static void
known_kitty_name_test (void)
{
    Responder r = { 0 };

    /* E.g. Ghostty over SSH, where TERM_PROGRAM doesn't make it across */
    r.version_reply = "\033P>|ghostty 1.1.3\033\\";
    r.kitty_reply = KITTY_OK_REPLY;

    g_assert_cmpint (run_probe ("TERM=xterm-256color", &r, NULL), ==, CHAFA_PIXEL_MODE_KITTY);
    g_assert_true (r.saw_version_query);
    g_assert_false (r.saw_kitty_query);
}

static void
unknown_error_test (void)
{
    Responder r = { 0 };
    gint n_input;

    r.version_reply = "\033P>|newterm 1.0\033\\";
    r.kitty_reply = KITTY_ERROR_REPLY;

    g_assert_cmpint (run_probe ("TERM=xterm-256color", &r, &n_input), ==, CHAFA_PIXEL_MODE_SYMBOLS);
    g_assert_true (r.saw_kitty_query);
    g_assert_cmpint (n_input, ==, 0);
}

static void
identified_term_test (void)
{
    Responder r = { 0 };

    r.version_reply = "\033P>|newterm 1.0\033\\";
    r.kitty_reply = KITTY_OK_REPLY;

    g_assert_cmpint (run_probe ("TERM=foot", &r, NULL), !=, CHAFA_PIXEL_MODE_KITTY);
    g_assert_false (r.saw_version_query);
    g_assert_false (r.saw_kitty_query);
}

static void
unknown_silent_test (void)
{
    Responder r = { 0 };

    r.version_reply = "\033P>|newterm 1.0\033\\";

    g_assert_cmpint (run_probe ("TERM=xterm-256color", &r, NULL), ==, CHAFA_PIXEL_MODE_SYMBOLS);
    g_assert_true (r.saw_kitty_query);
}

static void
known_name_test (void)
{
    Responder r = { 0 };

    r.version_reply = "\033P>|XTerm(390)\033\\";
    r.kitty_reply = KITTY_OK_REPLY;

    g_assert_cmpint (run_probe ("TERM=xterm-256color", &r, NULL), ==, CHAFA_PIXEL_MODE_SYMBOLS);
    g_assert_true (r.saw_version_query);
    g_assert_false (r.saw_kitty_query);
}

static void
deny_listed_test (void)
{
    Responder r = { 0 };

    r.version_reply = "\033P>|terminology(1.14.0)\033\\";
    r.kitty_reply = KITTY_OK_REPLY;

    g_assert_cmpint (run_probe ("TERM=xterm-256color", &r, NULL), ==, CHAFA_PIXEL_MODE_SYMBOLS);
    g_assert_false (r.saw_kitty_query);
}

static void
no_version_test (void)
{
    Responder r = { 0 };

    r.kitty_reply = KITTY_OK_REPLY;

    g_assert_cmpint (run_probe ("TERM=xterm-256color", &r, NULL), ==, CHAFA_PIXEL_MODE_SYMBOLS);
    g_assert_true (r.saw_version_query);
    g_assert_false (r.saw_kitty_query);
}

static void
db_kitty_test (void)
{
    Responder r = { 0 };

    r.version_reply = "\033P>|kitty(0.40.0)\033\\";
    r.kitty_reply = KITTY_OK_REPLY;

    g_assert_cmpint (run_probe ("TERM=xterm-kitty", &r, NULL), ==, CHAFA_PIXEL_MODE_KITTY);
    g_assert_false (r.saw_version_query);
    g_assert_false (r.saw_kitty_query);
}

static void
kitty_support_test (void)
{
    g_assert_cmpint (chafa_term_db_get_kitty_support ("newterm 1.0"), ==, CHAFA_KITTY_SUPPORT_UNKNOWN);
    g_assert_cmpint (chafa_term_db_get_kitty_support ("rex"), ==, CHAFA_KITTY_SUPPORT_UNKNOWN);

    g_assert_cmpint (chafa_term_db_get_kitty_support (NULL), ==, CHAFA_KITTY_SUPPORT_NO);
    g_assert_cmpint (chafa_term_db_get_kitty_support (""), ==, CHAFA_KITTY_SUPPORT_NO);
    g_assert_cmpint (chafa_term_db_get_kitty_support (" leading space"), ==, CHAFA_KITTY_SUPPORT_NO);

    g_assert_cmpint (chafa_term_db_get_kitty_support ("kitty(0.40.0)"), ==, CHAFA_KITTY_SUPPORT_YES);
    g_assert_cmpint (chafa_term_db_get_kitty_support ("ghostty 1.1.3"), ==, CHAFA_KITTY_SUPPORT_YES);

    g_assert_cmpint (chafa_term_db_get_kitty_support ("WezTerm 20240203-110809-5046fc22"), ==, CHAFA_KITTY_SUPPORT_NO);
    g_assert_cmpint (chafa_term_db_get_kitty_support ("XTerm(390)"), ==, CHAFA_KITTY_SUPPORT_NO);
    g_assert_cmpint (chafa_term_db_get_kitty_support ("tmux 3.4"), ==, CHAFA_KITTY_SUPPORT_NO);
    g_assert_cmpint (chafa_term_db_get_kitty_support ("iTerm2 3.5.0"), ==, CHAFA_KITTY_SUPPORT_NO);

    /* Terminals that print APC strings */
    g_assert_cmpint (chafa_term_db_get_kitty_support ("terminology(1.14.0)"), ==, CHAFA_KITTY_SUPPORT_NO);
    g_assert_cmpint (chafa_term_db_get_kitty_support ("RLogin 2.31.8"), ==, CHAFA_KITTY_SUPPORT_NO);
    g_assert_cmpint (chafa_term_db_get_kitty_support ("Kermit 95 3.0.0"), ==, CHAFA_KITTY_SUPPORT_NO);
    g_assert_cmpint (chafa_term_db_get_kitty_support ("K95(3.0.0)"), ==, CHAFA_KITTY_SUPPORT_NO);
}

static void
generic_term_name_test (void)
{
    g_assert_true (chafa_term_db_is_generic_term_name ("xterm"));
    g_assert_true (chafa_term_db_is_generic_term_name ("xterm-256color"));
    g_assert_true (chafa_term_db_is_generic_term_name ("vt220"));

    g_assert_false (chafa_term_db_is_generic_term_name (NULL));
    g_assert_false (chafa_term_db_is_generic_term_name ("xterm2"));
    g_assert_false (chafa_term_db_is_generic_term_name ("linux"));
    g_assert_false (chafa_term_db_is_generic_term_name ("foot"));
}

static gboolean
have_pty (void)
{
    gint fd = posix_openpt (O_RDWR | O_NOCTTY);

    if (fd < 0)
        return FALSE;

    close (fd);
    return TRUE;
}

int
main (int argc, char *argv [])
{
    if (!have_pty ())
        return 77;  /* Skip */

    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/term-probe/kitty-support", kitty_support_test);
    g_test_add_func ("/term-probe/generic-term-name", generic_term_name_test);
    g_test_add_func ("/term-probe/unknown-supported", unknown_supported_test);
    g_test_add_func ("/term-probe/unknown-error", unknown_error_test);
    g_test_add_func ("/term-probe/unknown-silent", unknown_silent_test);
    g_test_add_func ("/term-probe/known-name", known_name_test);
    g_test_add_func ("/term-probe/known-kitty-name", known_kitty_name_test);
    g_test_add_func ("/term-probe/identified-term", identified_term_test);
    g_test_add_func ("/term-probe/deny-listed", deny_listed_test);
    g_test_add_func ("/term-probe/no-version", no_version_test);
    g_test_add_func ("/term-probe/db-kitty", db_kitty_test);

    return g_test_run ();
}

#else

int
main (int argc, char *argv [])
{
    return 77;  /* Skip */
}

#endif
