/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2025 Hans Petter Jansson
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
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <chafa.h>
#include <nsgif.h>
#include "chicle-gif-loader.h"

#define BYTES_PER_PIXEL 4
#define IMAGE_BUFFER_SIZE_MAX (0xffffffffU >> 2)

struct ChicleGifLoader
{
    ChicleFileMapping *mapping;
    const guint8 *file_data;
    size_t file_data_len;
    nsgif_t *gif;
    const nsgif_info_t *info;
    nsgif_bitmap_t *frame_bitmap;
    gint current_frame_index;
    guint frame_is_decoded : 1;
    guint frame_is_success : 1;
};

static nsgif_bitmap_t *
bitmap_create (int width, int height)
{
    if ((width * (gint64) height * BYTES_PER_PIXEL) > IMAGE_BUFFER_SIZE_MAX)
        return NULL;

    return g_malloc0 (width * height * BYTES_PER_PIXEL);
}

static void
bitmap_destroy (nsgif_bitmap_t *bitmap)
{
    g_assert (bitmap != NULL);
    g_free (bitmap);
}

static uint8_t *
bitmap_get_buffer (nsgif_bitmap_t *bitmap)
{
    g_assert (bitmap != NULL);
    return bitmap;
}

static gboolean
maybe_decode_frame (ChicleGifLoader *loader)
{
    nsgif_error code;

    if (loader->frame_is_decoded)
        return loader->frame_is_success;

    code = nsgif_frame_decode (loader->gif, loader->current_frame_index,
                               &loader->frame_bitmap);

    loader->frame_is_decoded = TRUE;
    loader->frame_is_success = (code == NSGIF_OK ? TRUE : FALSE);

    return loader->frame_is_success;
}

static ChicleGifLoader *
chicle_gif_loader_new (void)
{
    return g_new0 (ChicleGifLoader, 1);
}

ChicleGifLoader *
chicle_gif_loader_new_from_mapping (ChicleFileMapping *mapping)
{
    static const nsgif_bitmap_cb_vt bitmap_callbacks =
    {
        bitmap_create,
        bitmap_destroy,
        bitmap_get_buffer,
        NULL,  /* set_opaque */
        NULL,  /* test_opaque */
        NULL,  /* modified */
        NULL   /* get_rowspan */
    };
    ChicleGifLoader *loader = NULL;
    gboolean success = FALSE;

    g_return_val_if_fail (mapping != NULL, NULL);

    if (!chicle_file_mapping_has_magic (mapping, 0, "GIF89a", 6)
        && !chicle_file_mapping_has_magic (mapping, 0, "GIF87a", 6))
        goto out;

    loader = chicle_gif_loader_new ();
    loader->mapping = mapping;

    loader->file_data = chicle_file_mapping_get_data (loader->mapping, &loader->file_data_len);
    if (!loader->file_data)
        goto out;

    if (nsgif_create (&bitmap_callbacks, NSGIF_BITMAP_FMT_R8G8B8A8, &loader->gif) != NSGIF_OK)
        goto out;

    /* Ignore scan errors - some of the frames may still be recovered */
    nsgif_data_scan (loader->gif, loader->file_data_len, loader->file_data);
    nsgif_data_complete (loader->gif);

    loader->info = nsgif_get_info (loader->gif);
    if (loader->info->frame_count < 1)
        goto out;

    /* Ensure we can decode a frame. If not, we can try other loaders */
    if (!maybe_decode_frame (loader))
        goto out;

    success = TRUE;

out:
    if (!success)
    {
        if (loader)
        {
            if (loader->gif)
                nsgif_destroy (loader->gif);

            g_free (loader);
            loader = NULL;
        }
    }

    return loader;
}

void
chicle_gif_loader_destroy (ChicleGifLoader *loader)
{
    if (loader->mapping)
        chicle_file_mapping_destroy (loader->mapping);

    if (loader->gif)
        nsgif_destroy (loader->gif);

    g_free (loader);
}

gboolean
chicle_gif_loader_get_is_animation (ChicleGifLoader *loader)
{
    g_return_val_if_fail (loader != NULL, 0);
    g_return_val_if_fail (loader->gif != NULL, 0);

    return loader->info->frame_count > 1 ? TRUE : FALSE;
}

gconstpointer
chicle_gif_loader_get_frame_data (ChicleGifLoader *loader,
                                  ChafaPixelType *pixel_type_out,
                                  gint *width_out,
                                  gint *height_out,
                                  gint *rowstride_out)
{
    g_return_val_if_fail (loader != NULL, NULL);
    g_return_val_if_fail (loader->gif != NULL, NULL);

    if (!maybe_decode_frame (loader))
        return NULL;

    if (width_out)
        *width_out = loader->info->width;
    if (height_out)
        *height_out = loader->info->height;
    if (pixel_type_out)
        *pixel_type_out = CHAFA_PIXEL_RGBA8_UNASSOCIATED;
    if (rowstride_out)
        *rowstride_out = loader->info->width * 4;

    return loader->frame_bitmap;
}

gint
chicle_gif_loader_get_frame_delay (ChicleGifLoader *loader)
{
    const nsgif_frame_info_t *frame_info;
    gint frame_delay_ms;

    g_return_val_if_fail (loader != NULL, 0);
    g_return_val_if_fail (loader->gif != NULL, 0);

    if (!maybe_decode_frame (loader))
        return 0;

    frame_info = nsgif_get_frame_info (loader->gif, loader->current_frame_index);
    if (!frame_info)
        return 0;

    /* Convert from centiseconds to milliseconds */
    frame_delay_ms = frame_info->delay * 10;

    /* It's common for GIF animations to omit the frame delays. If it looks like that's
     * what's happening, go with a 20fps default. */
    if (frame_delay_ms == 0)
        frame_delay_ms = 50;

    return frame_delay_ms;
}

void
chicle_gif_loader_goto_first_frame (ChicleGifLoader *loader)
{
    g_return_if_fail (loader != NULL);
    g_return_if_fail (loader->gif != NULL);

    if (loader->current_frame_index == 0)
        return;

    loader->current_frame_index = 0;
    loader->frame_is_decoded = FALSE;
    loader->frame_is_success = FALSE;
}

gboolean
chicle_gif_loader_goto_next_frame (ChicleGifLoader *loader)
{
    g_return_val_if_fail (loader != NULL, FALSE);
    g_return_val_if_fail (loader->gif != NULL, FALSE);

    if (loader->current_frame_index + 1 >= (gint) loader->info->frame_count)
        return FALSE;

    loader->current_frame_index++;
    loader->frame_is_decoded = FALSE;
    return TRUE;
}
