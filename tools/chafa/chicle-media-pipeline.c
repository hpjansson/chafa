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
#include "chicle-media-pipeline.h"

#define DEBUG(x)

typedef struct
{
    gchar *path;
    ChicleMediaLoader *loader;
    GString **output;
    GError *error;
}
Slot;

struct ChicleMediaPipeline
{
    ChiclePathQueue *path_queue;
    GThreadPool *thread_pool;
    ChafaTermInfo *term_info;
    ChafaCanvasConfig *canvas_config;
    GMutex mutex;
    GCond cond;
    Slot *slot_ring;
    gint first_slot;
    gint n_slots;
    gint target_width, target_height;
    ChafaAlign halign, valign;
    ChafaTuck tuck;
    guint want_loader : 1;
    guint want_output : 1;
};

static ChafaCanvas *
build_canvas (ChafaPixelType pixel_type, const guint8 *pixels,
              gint src_width, gint src_height, gint src_rowstride,
              const ChafaCanvasConfig *config,
              gint placement_id,
              ChafaAlign halign,
              ChafaAlign valign,
              ChafaTuck tuck)
{
    ChafaFrame *frame;
    ChafaImage *image;
    ChafaPlacement *placement;
    ChafaCanvas *canvas;

    canvas = chafa_canvas_new (config);
    frame = chafa_frame_new_borrow (pixels, pixel_type,
                                    src_width, src_height, src_rowstride);
    image = chafa_image_new ();
    chafa_image_set_frame (image, frame);

    placement = chafa_placement_new (image, placement_id);
    chafa_placement_set_tuck (placement, tuck);
    chafa_placement_set_halign (placement, halign);
    chafa_placement_set_valign (placement, valign);
    chafa_canvas_set_placement (canvas, placement);

    chafa_placement_unref (placement);
    chafa_image_unref (image);
    chafa_frame_unref (frame);
    return canvas;
}

static GString **
format_image (ChicleMediaPipeline *pipeline, ChicleMediaLoader *loader)
{
    ChafaPixelType pixel_type;
    gconstpointer pixels;
    ChafaCanvas *canvas = NULL;
    gint src_width, src_height, src_rowstride;
    GString **output = NULL;

    pixels = chicle_media_loader_get_frame_data (loader,
                                                 &pixel_type,
                                                 &src_width,
                                                 &src_height,
                                                 &src_rowstride);
    if (!pixels)
        goto out;

    canvas = build_canvas (pixel_type, pixels,
                           src_width, src_height, src_rowstride,
                           pipeline->canvas_config,
                           -1,
                           pipeline->halign, pipeline->valign,
                           pipeline->tuck);
    chafa_canvas_print_rows (canvas, pipeline->term_info, &output, NULL);

out:
    if (canvas)
        chafa_canvas_unref (canvas);
    return output;
}

static void
thread_func (gpointer ctx, gpointer data)
{
    ChicleMediaPipeline *pipeline = data;
    Slot *slot = ctx;
    ChicleMediaLoader *loader;
    GString **output = NULL;
    GError *error = NULL;

    loader = chicle_media_loader_new (slot->path,
                                      pipeline->target_width,
                                      pipeline->target_height,
                                      &error);

    if (loader
        && pipeline->want_output
        && pipeline->canvas_config
        && pipeline->term_info)
    {
        output = format_image (pipeline, loader);
    }

    if (loader
        && !pipeline->want_loader)
    {
        /* Destroy loaders early to reduce peak memory footprint */
        chicle_media_loader_destroy (loader);
        loader = NULL;
    }

    g_mutex_lock (&pipeline->mutex);

    DEBUG (g_printerr ("Push %s\n", slot->path));
    slot->loader = loader;
    slot->output = output;
    slot->error = error;

    g_cond_broadcast (&pipeline->cond);
    g_mutex_unlock (&pipeline->mutex);
}

static gint
nth_slot (ChicleMediaPipeline *pipeline, gint n)
{
    return (pipeline->first_slot + n) % (pipeline->n_slots);
}

static void
fill_pipeline (ChicleMediaPipeline *pipeline)
{
    gint i;

    for (i = 0; i < pipeline->n_slots; i++)
    {
        Slot *slot = &pipeline->slot_ring [nth_slot (pipeline, i)];

        if (slot->path)
            break;

        /* Get at least one path, more if available. If we get NULL from a
         * blocking pop, the path queue is done. */
        slot->path = chicle_path_queue_try_pop (pipeline->path_queue);
        if (!slot->path && i == 0)
            slot->path = chicle_path_queue_pop (pipeline->path_queue);
        if (!slot->path)
            break;

        DEBUG (g_printerr ("Req (%d) %s\n", nth_slot (pipeline, i), slot->path));
        g_thread_pool_push (pipeline->thread_pool, slot, NULL);
    }
}

static gboolean
wait_for_next (ChicleMediaPipeline *pipeline,
               gchar **path_out,
               ChicleMediaLoader **loader_out,
               GString ***output_out,
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

        if (slot->loader || slot->output || slot->error)
        {
            DEBUG (g_printerr ("Pop (%d) %s\n", nth_slot (pipeline, 0), slot->path));

            if (path_out)
                *path_out = slot->path;
            else
                g_free (slot->path);

            if (loader_out)
                *loader_out = slot->loader;
            else if (slot->loader)
                chicle_media_loader_destroy (slot->loader);

            if (output_out)
                *output_out = slot->output;
            else if (slot->output)
                chafa_free_gstring_array (slot->output);

            if (error_out)
                *error_out = slot->error;
            else if (slot->error)
                g_error_free (slot->error);

            memset (slot, 0, sizeof (*slot));
            pipeline->first_slot++;
            result = TRUE;
            break;
        }

        g_cond_wait (&pipeline->cond, &pipeline->mutex);
    }

    g_mutex_unlock (&pipeline->mutex);
    return result;
}

ChicleMediaPipeline *
chicle_media_pipeline_new (ChiclePathQueue *path_queue, gint target_width, gint target_height)
{
    ChicleMediaPipeline *pipeline;

    pipeline = g_new0 (ChicleMediaPipeline, 1);

    pipeline->n_slots = chafa_get_n_actual_threads ();
    pipeline->slot_ring = g_new0 (Slot, pipeline->n_slots);
    pipeline->path_queue = chicle_path_queue_ref (path_queue);
    pipeline->target_width = target_width;
    pipeline->target_height = target_height;
    pipeline->want_loader = TRUE;
    pipeline->want_output = TRUE;
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
chicle_media_pipeline_destroy (ChicleMediaPipeline *pipeline)
{
    gint i;

    g_thread_pool_free (pipeline->thread_pool, FALSE, TRUE);
    chicle_path_queue_unref (pipeline->path_queue);
    g_mutex_clear (&pipeline->mutex);
    g_cond_clear (&pipeline->cond);

    for (i = 0; i < pipeline->n_slots; i++)
    {
        Slot *slot = &pipeline->slot_ring [i];

        g_free (slot->path);
        if (slot->loader)
            chicle_media_loader_destroy (slot->loader);
        if (slot->output)
            chafa_free_gstring_array (slot->output);
        if (slot->error)
            g_error_free (slot->error);
    }

    if (pipeline->canvas_config)
        chafa_canvas_config_unref (pipeline->canvas_config);
    if (pipeline->term_info)
        chafa_term_info_unref (pipeline->term_info);
    g_free (pipeline->slot_ring);
    g_free (pipeline);
}

void
chicle_media_pipeline_set_want_loader (ChicleMediaPipeline *pipeline,
                                       gboolean want_loader)
{
    pipeline->want_loader = !!want_loader;
}

void
chicle_media_pipeline_set_want_output (ChicleMediaPipeline *pipeline,
                                       gboolean want_output)
{
    pipeline->want_output = !!want_output;
}

void
chicle_media_pipeline_set_formatting (ChicleMediaPipeline *pipeline,
                                      ChafaCanvasConfig *canvas_config,
                                      ChafaTermInfo *term_info,
                                      ChafaAlign halign,
                                      ChafaAlign valign,
                                      ChafaTuck tuck)
{
    ChafaCanvasConfig *old_canvas_config = pipeline->canvas_config;
    ChafaTermInfo *old_term_info = pipeline->term_info;

    if (canvas_config)
        chafa_canvas_config_ref (canvas_config);
    pipeline->canvas_config = canvas_config;
    if (old_canvas_config)
        chafa_canvas_config_unref (old_canvas_config);

    if (term_info)
        chafa_term_info_ref (term_info);
    pipeline->term_info = term_info;
    if (old_term_info)
        chafa_term_info_unref (old_term_info);

    pipeline->halign = halign;
    pipeline->valign = valign;
    pipeline->tuck = tuck;
}

gboolean
chicle_media_pipeline_pop (ChicleMediaPipeline *pipeline,
                           gchar **path_out,
                           ChicleMediaLoader **loader_out,
                           GString ***output_out,
                           GError **error_out)
{
    gboolean result;

    result = wait_for_next (pipeline, path_out, loader_out, output_out, error_out);
    return result;
}
