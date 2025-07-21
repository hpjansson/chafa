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
#include <chafa.h>
#include "media-pipeline.h"

#define DEBUG(x)

typedef struct
{
    gchar *path;
    MediaLoader *loader;
    GError *error;
}
Slot;

struct MediaPipeline
{
    PathQueue *path_queue;
    GThreadPool *thread_pool;
    GMutex mutex;
    GCond cond;
    Slot *slot_ring;
    gint first_slot;
    gint n_slots;
    gint target_width, target_height;
};

static void
thread_func (gpointer ctx, gpointer data)
{
    MediaPipeline *pipeline = data;
    Slot *slot = ctx;
    MediaLoader *loader;
    GError *error = NULL;

    loader = media_loader_new (slot->path,
                               pipeline->target_width,
                               pipeline->target_height,
                               &error);

    g_mutex_lock (&pipeline->mutex);
    DEBUG (g_printerr ("Push %s\n", slot->path));
    slot->loader = loader;
    slot->error = error;
    g_cond_broadcast (&pipeline->cond);
    g_mutex_unlock (&pipeline->mutex);
}

static gint
nth_slot (MediaPipeline *pipeline, gint n)
{
    return (pipeline->first_slot + n) % (pipeline->n_slots);
}

static void
fill_pipeline (MediaPipeline *pipeline)
{
    gint i;

    for (i = 0; i < pipeline->n_slots; i++)
    {
        Slot *slot = &pipeline->slot_ring [nth_slot (pipeline, i)];

        if (slot->path)
            break;

        slot->path = path_queue_pop (pipeline->path_queue);
        if (!slot->path)
            break;

        DEBUG (g_printerr ("Req (%d) %s\n", nth_slot (pipeline, i), slot->path));
        g_thread_pool_push (pipeline->thread_pool, slot, NULL);
    }
}

static gboolean
wait_for_next (MediaPipeline *pipeline,
               gchar **path_out,
               MediaLoader **loader_out,
               GError **error_out)
{
    gboolean result = FALSE;

    g_mutex_lock (&pipeline->mutex);

    for (;;)
    {
        Slot *slot = &pipeline->slot_ring [nth_slot (pipeline, 0)];

        fill_pipeline (pipeline);

        if (!slot->path)
        {
            /* Reached end of path queue */
            DEBUG (g_printerr ("Reached end (%d)\n", nth_slot (pipeline, 0)));
            break;
        }

        if (slot->loader || slot->error)
        {
            DEBUG (g_printerr ("Pop (%d) %s\n", nth_slot (pipeline, 0), slot->path));

            if (path_out)
                *path_out = slot->path;
            else
                g_free (slot->path);

            if (loader_out)
                *loader_out = slot->loader;
            else
                media_loader_destroy (slot->loader);

            if (error_out)
                *error_out = slot->error;
            else if (slot->error)
                g_error_free (slot->error);

            memset (slot, 0, sizeof (*slot));
            fill_pipeline (pipeline);
            pipeline->first_slot++;
            result = TRUE;
            break;
        }

        g_cond_wait (&pipeline->cond, &pipeline->mutex);
    }

    g_mutex_unlock (&pipeline->mutex);
    return result;
}

MediaPipeline *
media_pipeline_new (PathQueue *path_queue, gint target_width, gint target_height)
{
    MediaPipeline *pipeline;

    pipeline = g_new0 (MediaPipeline, 1);

    pipeline->n_slots = chafa_get_n_actual_threads ();
    pipeline->slot_ring = g_new0 (Slot, pipeline->n_slots);
    pipeline->path_queue = path_queue_ref (path_queue);
    pipeline->target_width = target_width;
    pipeline->target_height = target_height;
    g_mutex_init (&pipeline->mutex);
    g_cond_init (&pipeline->cond);
    pipeline->thread_pool = g_thread_pool_new ((GFunc) thread_func,
                                               (gpointer) pipeline,
                                               pipeline->n_slots,
                                               FALSE,
                                               NULL);
    return pipeline;
}

void
media_pipeline_destroy (MediaPipeline *pipeline)
{
    gint i;

    g_thread_pool_free (pipeline->thread_pool, FALSE, TRUE);
    path_queue_unref (pipeline->path_queue);
    g_mutex_clear (&pipeline->mutex);
    g_cond_clear (&pipeline->cond);

    for (i = 0; i < pipeline->n_slots; i++)
    {
        Slot *slot = &pipeline->slot_ring [i];

        g_free (slot->path);
        if (slot->loader)
            media_loader_destroy (slot->loader);
        if (slot->error)
            g_error_free (slot->error);
    }

    g_free (pipeline->slot_ring);
    g_free (pipeline);
}

gboolean
media_pipeline_pop (MediaPipeline *pipeline,
                    gchar **path_out,
                    MediaLoader **loader_out,
                    GError **error_out)
{
    gboolean result;

    result = wait_for_next (pipeline, path_out, loader_out, error_out);
    return result;
}
