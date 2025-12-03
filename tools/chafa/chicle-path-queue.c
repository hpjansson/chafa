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
#include <fcntl.h>  /* open */
#include <unistd.h>  /* STDIN_FILENO */

#include "chafa.h"
#include "chicle-path-queue.h"

/* Include after glib.h for G_OS_WIN32 */
#ifdef G_OS_WIN32
# ifdef HAVE_WINDOWS_H
#  include <windows.h>
# endif
# include <wchar.h>
# include <io.h>
#endif

/* ------------------- *
 * Defines and structs *
 * ------------------- */

#define PATH_TOKEN_LEN_MAX 16384

typedef enum
{
    PATH_SOURCE_PATH,
    PATH_SOURCE_PATH_LIST,
    PATH_SOURCE_STREAM
}
PathSourceType;

typedef struct
{
    PathSourceType type;
    gpointer data;

    /* Element separator for PATH_SOURCE_STREAM */
    gchar *separator;
    gint separator_len;
}
PathSource;

struct ChiclePathQueue
{
    gint refs;

    GQueue *queue;
    PathSource *current_src;
    ChafaStreamReader *current_reader;
    gchar *current_path_token;
    gint n_processed;

    guint have_stdin_source : 1;
};

static void
path_source_destroy (PathSource *src)
{
    if (!src)
        return;

    switch (src->type)
    {
        case PATH_SOURCE_PATH:
            g_free (src->data);
            break;
        case PATH_SOURCE_PATH_LIST:
            g_list_free_full (src->data, g_free);
            break;
        case PATH_SOURCE_STREAM:
            g_free (src->data);
            g_free (src->separator);
            break;
    }

    g_free (src);
}

static gboolean
path_source_is_stdin (PathSource *src)
{
    return (src->type == PATH_SOURCE_STREAM
            && !strcmp (src->data, "-")) ? TRUE : FALSE;
}

static void
maybe_close_fd (ChafaStreamReader *stream_reader)
{
    gint fd = chafa_stream_reader_get_fd (stream_reader);
    if (fd != STDIN_FILENO)
        close (fd);
}

static void
clear_current_src (ChiclePathQueue *path_queue)
{
    path_source_destroy (path_queue->current_src);
    path_queue->current_src = NULL;
    if (path_queue->current_reader)
    {
        maybe_close_fd (path_queue->current_reader);
        chafa_stream_reader_unref (path_queue->current_reader);
        path_queue->current_reader = NULL;
    }
}

/* Check if we've reached the end of current_src. Clear it if so. */
static void
check_src_end (ChiclePathQueue *path_queue)
{
    if (!path_queue->current_src)
        return;

    switch (path_queue->current_src->type)
    {
        case PATH_SOURCE_PATH:
            break;
        case PATH_SOURCE_PATH_LIST:
            if (!path_queue->current_src->data)
                clear_current_src (path_queue);
            break;
        case PATH_SOURCE_STREAM:
            if (!path_queue->current_reader
                || chafa_stream_reader_is_eof (path_queue->current_reader))
                clear_current_src (path_queue);
            break;
    }
}

static void
open_current_stream (ChiclePathQueue *path_queue)
{
    gint fd;

    g_assert (path_queue->current_reader == NULL);
    g_assert (path_queue->current_src->type == PATH_SOURCE_STREAM);

    fd = path_source_is_stdin (path_queue->current_src) ? STDIN_FILENO
        : open (path_queue->current_src->data, O_RDONLY);

    if (fd >= 0)
        path_queue->current_reader = chafa_stream_reader_new_from_fd_full (
            fd, path_queue->current_src->separator, path_queue->current_src->separator_len);
}

/* Ensure there's a current_src and that it's not empty. For streams, this
 * may be unknown until async reads have finished; we return TRUE for these. */
static gboolean
ensure_current_src (ChiclePathQueue *path_queue)
{
    for (;;)
    {
        if (!path_queue->current_src)
        {
            path_queue->current_src = g_queue_pop_head (path_queue->queue);
            if (!path_queue->current_src)
                break;

            if (path_queue->current_src->type == PATH_SOURCE_STREAM)
                open_current_stream (path_queue);
        }

        check_src_end (path_queue);
        if (path_queue->current_src)
            return TRUE;
    }

    return FALSE;
}

static void
destroy_path_source (gpointer data)
{
    PathSource *src = data;

    switch (src->type)
    {
        case PATH_SOURCE_PATH:
            g_free (src->data);
            break;

        case PATH_SOURCE_PATH_LIST:
            g_list_free_full (src->data, g_free);
            break;

        case PATH_SOURCE_STREAM:
            g_free (src->data);
            g_free (src->separator);
            break;
    }
}

static gboolean
pop_stream_path_token (ChiclePathQueue *path_queue)
{
    gint result = 0;

    g_assert (path_queue->current_src->type == PATH_SOURCE_STREAM);

    if (path_queue->current_path_token)
        return TRUE;

    /* Discard blank lines until we encounter a non-blank line or an error */
    while (result == 0)
    {
        g_free (path_queue->current_path_token);
        path_queue->current_path_token = NULL;

        result = chafa_stream_reader_read_token (path_queue->current_reader,
                                                 (gpointer *) &path_queue->current_path_token,
                                                 PATH_TOKEN_LEN_MAX);
        if (result > 0 && !strcmp (path_queue->current_src->separator, "\n"))
        {
            /* If we're separating on \n, handle \r\n by eliminating the \r too */
            if (path_queue->current_path_token [result - 1] == '\r')
            {
                path_queue->current_path_token [result - 1] = '\0';
                result--;
            }
        }
    }

    return result >= 0 ? TRUE : FALSE;
}

/* ---------- *
 * Public API *
 * ---------- */

ChiclePathQueue *
chicle_path_queue_new (void)
{
    ChiclePathQueue *path_queue;

    path_queue = g_new0 (ChiclePathQueue, 1);
    path_queue->refs = 1;
    path_queue->queue = g_queue_new ();

    return path_queue;
}

ChiclePathQueue *
chicle_path_queue_ref (ChiclePathQueue *path_queue)
{
    gint refs;

    g_return_val_if_fail (path_queue != NULL, NULL);
    refs = g_atomic_int_get (&path_queue->refs);
    g_return_val_if_fail (refs > 0, NULL);

    g_atomic_int_inc (&path_queue->refs);
    return path_queue;
}

void
chicle_path_queue_unref (ChiclePathQueue *path_queue)
{
    gint refs;

    g_return_if_fail (path_queue != NULL);
    refs = g_atomic_int_get (&path_queue->refs);
    g_return_if_fail (refs > 0);

    if (g_atomic_int_dec_and_test (&path_queue->refs))
    {
        clear_current_src (path_queue);
        g_queue_free_full (path_queue->queue, destroy_path_source);
        g_free (path_queue);
    }
}

void
chicle_path_queue_push_path (ChiclePathQueue *path_queue, const gchar *path)
{
    PathSource *src;

    src = g_new0 (PathSource, 1);
    src->type = PATH_SOURCE_PATH;
    src->data = g_strdup (path);

    g_queue_push_tail (path_queue->queue, src);
}

void
chicle_path_queue_push_path_list_steal (ChiclePathQueue *path_queue, GList *path_list)
{
    PathSource *src;

    src = g_new0 (PathSource, 1);
    src->type = PATH_SOURCE_PATH_LIST;
    src->data = path_list;

    g_queue_push_tail (path_queue->queue, src);
}

void
chicle_path_queue_push_stream (ChiclePathQueue *path_queue, const gchar *stream_path,
                               const gchar *separator, gint separator_len)
{
    PathSource *src;

    src = g_new0 (PathSource, 1);
    src->type = PATH_SOURCE_STREAM;
    src->data = g_strdup (stream_path);
    src->separator = g_malloc (separator_len + 1);
    src->separator_len = separator_len;
    memcpy (src->separator, separator, separator_len);
    src->separator [separator_len] = '\0';

    g_queue_push_tail (path_queue->queue, src);

    if (path_source_is_stdin (src))
        path_queue->have_stdin_source = TRUE;
}

void
chicle_path_queue_wait (ChiclePathQueue *path_queue)
{
    if (!ensure_current_src (path_queue))
        return;

    if (path_queue->current_src->type == PATH_SOURCE_STREAM)
    {
        if (pop_stream_path_token (path_queue))
            return;

        chafa_stream_reader_wait (path_queue->current_reader, -1);
    }
}

gchar *
chicle_path_queue_try_pop (ChiclePathQueue *path_queue)
{
    gchar *path = NULL;
    GList *l;

    if (!ensure_current_src (path_queue))
        return NULL;

    switch (path_queue->current_src->type)
    {
        case PATH_SOURCE_PATH:
            path = path_queue->current_src->data;
            clear_current_src (path_queue);
            break;
        case PATH_SOURCE_PATH_LIST:
            l = path_queue->current_src->data;
            path = l->data;
            path_queue->current_src->data =
                g_list_delete_link (path_queue->current_src->data, l);
            break;
        case PATH_SOURCE_STREAM:
            pop_stream_path_token (path_queue);
            if (path_queue->current_path_token)
            {
                path = path_queue->current_path_token;
                path_queue->current_path_token = NULL;
            }
            break;
    }

    if (path)
        path_queue->n_processed++;
    return path;
}

gchar *
chicle_path_queue_pop (ChiclePathQueue *path_queue)
{
    gchar *path = NULL;

    for (;;)
    {
        path = chicle_path_queue_try_pop (path_queue);
        if (path)
            break;

        if (chicle_path_queue_is_empty (path_queue))
            break;

        chicle_path_queue_wait (path_queue);
    }

    return path;
}

gint
chicle_path_queue_get_length (ChiclePathQueue *path_queue)
{
    return g_queue_get_length (path_queue->queue);
}

gboolean
chicle_path_queue_is_empty (ChiclePathQueue *path_queue)
{
    if (!ensure_current_src (path_queue))
        return TRUE;

    return FALSE;
}

gint
chicle_path_queue_get_n_processed (ChiclePathQueue *path_queue)
{
    return path_queue->n_processed;
}

gboolean
chicle_path_queue_have_stdin_source (ChiclePathQueue *path_queue)
{
    return path_queue->have_stdin_source ? TRUE : FALSE;
}
