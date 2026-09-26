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
#include <stdio.h>
#include <string.h>

#ifndef G_OS_WIN32

#include <unistd.h>

/* Every wait in this test is bounded so that a regression in the reader's
 * wakeup logic fails instead of hanging the suite. */
#define WAIT_TIMEOUT_US (10 * G_USEC_PER_SEC)

/* Bigger than the reader's internal buffer cap plus the pipe buffer, forcing
 * the reader thread to stall on backpressure when a separator has yet to be
 * found. */
#define BIG_TOKEN_LEN 100000

typedef struct
{
    gint fd;
    const gchar *data;
    gsize len;
}
WriterArgs;

static gpointer
writer_thread (gpointer data)
{
    WriterArgs *args = data;
    gsize written = 0;

    while (written < args->len)
    {
        gssize n = write (args->fd, args->data + written, args->len - written);

        g_assert (n > 0);
        written += n;
    }

    close (args->fd);
    return args;
}

/* Feed the reader from a pipe. Small payloads are written synchronously,
 * large ones from a thread so the writer can't fill the pipe and stall. */
static ChafaStreamReader *
build_reader (const gchar *data, gsize len, const gchar *sep, gint max_token_len,
              GThread **writer_out)
{
    ChafaStreamReader *reader;
    gint fds [2];
    gint r;

    r = pipe (fds);
    g_assert (r == 0);

    if (sep)
        reader = chafa_stream_reader_new_from_fd_full (fds [0], sep, strlen (sep), max_token_len);
    else
        reader = chafa_stream_reader_new_from_fd (fds [0]);

    if (len < 4096)
    {
        WriterArgs args = { fds [1], data, len };

        writer_thread (&args);
        *writer_out = NULL;
    }
    else
    {
        WriterArgs *args = g_new (WriterArgs, 1);

        args->fd = fds [1];
        args->data = data;
        args->len = len;
        *writer_out = g_thread_new ("writer", writer_thread, args);
    }

    return reader;
}

static void
finish_reader (ChafaStreamReader *reader, GThread *writer)
{
    gint fd = chafa_stream_reader_get_fd (reader);

    if (writer)
        g_free (g_thread_join (writer));

    chafa_stream_reader_unref (reader);
    close (fd);
}

/* Wait for a token with a bounded timeout. Returns the token length, an
 * error code, or CHAFA_STREAM_READER_ERROR_NO_DATA at EOF. */
static gint
read_token_blocking (ChafaStreamReader *reader, gchar **token_out)
{
    for (;;)
    {
        gint result;
        gboolean woke;

        woke = chafa_stream_reader_wait_until (reader,
                                               g_get_monotonic_time () + WAIT_TIMEOUT_US);
        g_assert_true (woke);

        result = chafa_stream_reader_read_token (reader, (gpointer *) token_out);
        if (result != CHAFA_STREAM_READER_ERROR_NO_DATA)
            return result;
        if (chafa_stream_reader_is_eof (reader))
            return result;
    }
}

static void
assert_token (ChafaStreamReader *reader, const gchar *expected)
{
    gchar *token = NULL;
    gint result;

    result = read_token_blocking (reader, &token);
    g_assert_cmpint (result, ==, (gint) strlen (expected));
    g_assert_nonnull (token);
    g_assert_cmpstr (token, ==, expected);
    g_free (token);
}

static void
assert_result (ChafaStreamReader *reader, gint expected)
{
    gchar *token = (gchar *) 0x1;
    gint result;

    result = read_token_blocking (reader, &token);
    g_assert_cmpint (result, ==, expected);

    /* Out pointer must be left untouched on error */
    g_assert_true (token == (gchar *) 0x1);
}

static void
assert_eof (ChafaStreamReader *reader)
{
    assert_result (reader, CHAFA_STREAM_READER_ERROR_NO_DATA);
    g_assert_true (chafa_stream_reader_is_eof (reader));
}

static void
zero_length_tokens_test (void)
{
    ChafaStreamReader *reader;
    GThread *writer;

    reader = build_reader ("\n\nabc\n\n", 7, "\n", -1, &writer);
    assert_token (reader, "");
    assert_token (reader, "");
    assert_token (reader, "abc");
    assert_token (reader, "");
    assert_eof (reader);
    finish_reader (reader, writer);

    /* A limit of 0 admits empty tokens only */
    reader = build_reader ("\nab\n\n", 5, "\n", 0, &writer);
    assert_token (reader, "");
    assert_result (reader, CHAFA_STREAM_READER_ERROR_DISCARDED_TOKEN);
    assert_token (reader, "");
    assert_eof (reader);
    finish_reader (reader, writer);
}

static void
remainder_test (void)
{
    ChafaStreamReader *reader;
    GThread *writer;

    /* Data after the final separator is a token of its own */
    reader = build_reader ("abc\ndef", 7, "\n", -1, &writer);
    assert_token (reader, "abc");
    assert_token (reader, "def");
    assert_eof (reader);
    finish_reader (reader, writer);

    /* ...unless it's oversized */
    reader = build_reader ("abc\ndefg", 8, "\n", 3, &writer);
    assert_token (reader, "abc");
    assert_result (reader, CHAFA_STREAM_READER_ERROR_DISCARDED_TOKEN);
    assert_eof (reader);
    finish_reader (reader, writer);

    /* Empty stream */
    reader = build_reader ("", 0, "\n", -1, &writer);
    assert_eof (reader);
    finish_reader (reader, writer);
}

static void
multibyte_separator_test (void)
{
    ChafaStreamReader *reader;
    GThread *writer;

    reader = build_reader ("a--b----c-d--", 13, "--", -1, &writer);
    assert_token (reader, "a");
    assert_token (reader, "b");
    assert_token (reader, "");
    assert_token (reader, "c-d");
    assert_eof (reader);
    finish_reader (reader, writer);
}

static void
oversized_token_test (void)
{
    ChafaStreamReader *reader;
    GThread *writer;

    reader = build_reader ("abcd\nxy\nabc\n", 12, "\n", 3, &writer);
    assert_result (reader, CHAFA_STREAM_READER_ERROR_DISCARDED_TOKEN);
    assert_token (reader, "xy");
    assert_token (reader, "abc");
    assert_eof (reader);
    finish_reader (reader, writer);
}

/* A token longer than the reader's buffer cap must be skipped (or returned,
 * if no limit is set) rather than deadlocking the reader thread. */
static void
huge_token_test (void)
{
    ChafaStreamReader *reader;
    GThread *writer;
    gchar *data;
    gchar *token = NULL;
    gsize len;
    gint result;

    len = BIG_TOKEN_LEN + 4;
    data = g_malloc (len + 1);
    memset (data, 'a', BIG_TOKEN_LEN);
    memcpy (data + BIG_TOKEN_LEN, "\nok\n", 5);

    /* Discard with limit */

    reader = build_reader (data, len, "\n", 16, &writer);
    assert_result (reader, CHAFA_STREAM_READER_ERROR_DISCARDED_TOKEN);
    assert_token (reader, "ok");
    assert_eof (reader);
    finish_reader (reader, writer);

    /* Return whole with no limit */

    reader = build_reader (data, len, "\n", -1, &writer);
    result = read_token_blocking (reader, &token);
    g_assert_cmpint (result, ==, BIG_TOKEN_LEN);
    g_assert_nonnull (token);
    g_assert_true (token [0] == 'a' && token [BIG_TOKEN_LEN - 1] == 'a'
                   && token [BIG_TOKEN_LEN] == '\0');
    g_free (token);
    assert_token (reader, "ok");
    assert_eof (reader);
    finish_reader (reader, writer);

    /* Huge remainder with no trailing separator */

    reader = build_reader (data, BIG_TOKEN_LEN, "\n", 16, &writer);
    assert_result (reader, CHAFA_STREAM_READER_ERROR_DISCARDED_TOKEN);
    assert_eof (reader);
    finish_reader (reader, writer);

    g_free (data);
}

int
main (int argc, char *argv [])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/stream-reader/huge-token", huge_token_test);
    g_test_add_func ("/stream-reader/multibyte-separator", multibyte_separator_test);
    g_test_add_func ("/stream-reader/oversized-token", oversized_token_test);
    g_test_add_func ("/stream-reader/remainder", remainder_test);
    g_test_add_func ("/stream-reader/zero-length-tokens", zero_length_tokens_test);

    return g_test_run ();
}

#else /* G_OS_WIN32 */

int
main (void)
{
    /* Pipe plumbing here is POSIX-only; skip */
    return 77;
}

#endif
