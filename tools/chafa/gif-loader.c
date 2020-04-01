/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2020 Hans Petter Jansson
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
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libnsgif.h>
#include "gif-loader.h"

#define BYTES_PER_PIXEL 4
#define MAX_IMAGE_BYTES (128 * 1024 * 1024)

struct GifLoader
{
    FileMapping *mapping;
    const guint8 *file_data;
    size_t file_data_len;
    gif_animation gif;
    gif_result code;
    gint current_frame_index;
    guint gif_is_initialized : 1;
    guint frame_is_decoded : 1;
};

static void *
bitmap_create (int width, int height)
{
    /* ensure a stupidly large bitmap is not created */
    if (((long long) width * (long long) height) > (MAX_IMAGE_BYTES/BYTES_PER_PIXEL))
        return NULL;

    return g_malloc0 (width * height * BYTES_PER_PIXEL);
}

static void
bitmap_set_opaque (void *bitmap, bool opaque)
{
    (void) opaque;  /* unused */
    (void) bitmap;  /* unused */
    g_assert (bitmap);
}

static bool
bitmap_test_opaque (void *bitmap)
{
    (void) bitmap;  /* unused */
    g_assert (bitmap != NULL);
    return false;
}

static unsigned char *
bitmap_get_buffer (void *bitmap)
{
    g_assert (bitmap != NULL);
    return bitmap;
}

static void
bitmap_destroy (void *bitmap)
{
    g_assert (bitmap != NULL);
    g_free (bitmap);
}

static void
bitmap_modified (void *bitmap)
{
    (void) bitmap;  /* unused */
    g_assert (bitmap != NULL);
}

static GifLoader *
gif_loader_new (void)
{
    return g_new0 (GifLoader, 1);
}

GifLoader *
gif_loader_new_from_mapping (FileMapping *mapping)
{
    gif_bitmap_callback_vt bitmap_callbacks =
    {
        bitmap_create,
        bitmap_destroy,
        bitmap_get_buffer,
        bitmap_set_opaque,
        bitmap_test_opaque,
        bitmap_modified
    };
    gif_result code;
    GifLoader *loader = NULL;
    gboolean success = FALSE;

    g_return_val_if_fail (mapping != NULL, NULL);

    if (!file_mapping_has_magic (mapping, 0, "GIF89a", 6))
        goto out;

    loader = gif_loader_new ();
    loader->mapping = mapping;

    loader->file_data = file_mapping_get_data (loader->mapping, &loader->file_data_len);
    if (!loader->file_data)
        goto out;

    gif_create (&loader->gif, &bitmap_callbacks);
    loader->gif_is_initialized = TRUE;

    do
    {
        code = gif_initialise (&loader->gif, loader->file_data_len, (gpointer) loader->file_data);

        if (code != GIF_OK && code != GIF_WORKING)
            goto out;
    }
    while (code != GIF_OK);

    success = TRUE;

out:
    if (!success)
    {
        if (loader)
        {
            if (loader->gif_is_initialized)
                gif_finalise (&loader->gif);

            g_free (loader);
            loader = NULL;
        }
    }

    return loader;
}

void
gif_loader_destroy (GifLoader *loader)
{
    if (loader->mapping)
        file_mapping_destroy (loader->mapping);

    if (loader->gif_is_initialized)
        gif_finalise (&loader->gif);

    g_free (loader);
}

void
gif_loader_get_geometry (GifLoader *loader, gint *width_out, gint *height_out)
{
    g_return_if_fail (loader != NULL);
    g_return_if_fail (loader->gif_is_initialized);

    *width_out = loader->gif.width;
    *height_out = loader->gif.height;
}

gint
gif_loader_get_n_frames (GifLoader *loader)
{
    g_return_val_if_fail (loader != NULL, 0);
    g_return_val_if_fail (loader->gif_is_initialized, 0);

    return loader->gif.frame_count;
}

const guint8 *
gif_loader_get_frame_data (GifLoader *loader, gint *post_frame_delay_hs_out)
{
    g_return_val_if_fail (loader != NULL, NULL);
    g_return_val_if_fail (loader->gif_is_initialized, NULL);

    if (!loader->frame_is_decoded)
    {
        gif_result code = gif_decode_frame (&loader->gif, loader->current_frame_index);
        if (code != GIF_OK)
            return NULL;
    }

    loader->frame_is_decoded = TRUE;

    if (post_frame_delay_hs_out)
        *post_frame_delay_hs_out = loader->gif.frames [loader->current_frame_index].frame_delay;
    return loader->gif.frame_image;
}

void
gif_loader_first_frame (GifLoader *loader)
{
    g_return_if_fail (loader != NULL);
    g_return_if_fail (loader->gif_is_initialized);

    if (loader->current_frame_index == 0)
        return;

    loader->current_frame_index = 0;
    loader->frame_is_decoded = FALSE;
}

gboolean
gif_loader_next_frame (GifLoader *loader)
{
    g_return_val_if_fail (loader != NULL, FALSE);
    g_return_val_if_fail (loader->gif_is_initialized, FALSE);

    if (loader->current_frame_index + 1 >= (gint) loader->gif.frame_count)
        return FALSE;

    loader->current_frame_index++;
    loader->frame_is_decoded = FALSE;
    return TRUE;
}
