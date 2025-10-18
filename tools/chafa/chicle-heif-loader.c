/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2025 Hans Petter Jansson
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
#include <libheif/heif.h>
#include "chicle-heif-loader.h"

#define BYTES_PER_PIXEL 4
#define IMAGE_BUFFER_SIZE_MAX (0xffffffffU >> 2)

struct ChicleHeifLoader
{
    ChicleFileMapping *mapping;
    const guint8 *file_data;
    size_t file_data_len;
    gint width, height;
    gint stride;

    heif_context *ctx;
    heif_image_handle *handle;
    heif_image *image;
    const uint8_t *frame_data;
};

static void
free_heif_handles (ChicleHeifLoader *loader)
{
    if (loader->image)
        heif_image_release (loader->image);
    if (loader->handle)
        heif_image_handle_release (loader->handle);
    if (loader->ctx)
        heif_context_free (loader->ctx);

    loader->image = NULL;
    loader->handle = NULL;
    loader->ctx = NULL;
}

static ChicleHeifLoader *
chicle_heif_loader_new (void)
{
    return g_new0 (ChicleHeifLoader, 1);
}

ChicleHeifLoader *
chicle_heif_loader_new_from_mapping (ChicleFileMapping *mapping)
{
    ChicleHeifLoader *loader = NULL;
    gboolean success = FALSE;

    g_return_val_if_fail (mapping != NULL, NULL);

    /* Quick check for the ISOBMFF ftyp box to filter out files that are
     * something else entirely */
    if (!chicle_file_mapping_has_magic (mapping, 4, "ftyp", 4))
        goto out;

    loader = chicle_heif_loader_new ();
    loader->mapping = mapping;

    loader->file_data = chicle_file_mapping_get_data (loader->mapping, &loader->file_data_len);
    if (!loader->file_data)
        goto out;

    loader->ctx = heif_context_alloc ();
    if (!loader->ctx)
        goto out;

    if (heif_context_read_from_memory_without_copy (loader->ctx,
                                                    loader->file_data,
                                                    loader->file_data_len,
                                                    NULL).code
        != heif_error_Ok)
        goto out;

    heif_context_get_primary_image_handle (loader->ctx,
                                           &loader->handle);
    if (!loader->handle)
        goto out;

    heif_decode_image (loader->handle,
                       &loader->image,
                       heif_colorspace_RGB,
                       heif_chroma_interleaved_RGBA,
                       NULL);
    if (!loader->image)
        goto out;

    loader->width = heif_image_get_primary_width (loader->image);
    loader->height = heif_image_get_primary_height (loader->image);

    if (loader->width < 1 || loader->width >= (1 << 28)
        || loader->height < 1 || loader->height >= (1 << 28))
        goto out;

    if ((unsigned int) loader->width * loader->height * BYTES_PER_PIXEL > IMAGE_BUFFER_SIZE_MAX)
        goto out;

    loader->frame_data = heif_image_get_plane_readonly (loader->image,
                                                        heif_channel_interleaved,
                                                        &loader->stride);
    if (!loader->frame_data || loader->stride < 1)
        goto out;

    success = TRUE;

out:
    if (!success)
    {
        if (loader)
        {
            free_heif_handles (loader);
            g_free (loader);
            loader = NULL;
        }
    }

    return loader;
}

void
chicle_heif_loader_destroy (ChicleHeifLoader *loader)
{
    free_heif_handles (loader);

    if (loader->mapping)
        chicle_file_mapping_destroy (loader->mapping);

    g_free (loader);
}

gboolean
chicle_heif_loader_get_is_animation (ChicleHeifLoader *loader)
{
    g_return_val_if_fail (loader != NULL, 0);

    return FALSE;
}

gconstpointer
chicle_heif_loader_get_frame_data (ChicleHeifLoader *loader,
                                   ChafaPixelType *pixel_type_out,
                                   gint *width_out,
                                   gint *height_out,
                                   gint *rowstride_out)
{
    g_return_val_if_fail (loader != NULL, NULL);

    if (pixel_type_out)
    {
        *pixel_type_out = heif_image_is_premultiplied_alpha (loader->image)
            ? CHAFA_PIXEL_RGBA8_PREMULTIPLIED : CHAFA_PIXEL_RGBA8_UNASSOCIATED;
    }
    if (width_out)
        *width_out = loader->width;
    if (height_out)
        *height_out = loader->height;
    if (rowstride_out)
        *rowstride_out = loader->stride;

    return loader->frame_data;
}

gint
chicle_heif_loader_get_frame_delay (ChicleHeifLoader *loader)
{
    g_return_val_if_fail (loader != NULL, 0);

    return 0;
}

void
chicle_heif_loader_goto_first_frame (ChicleHeifLoader *loader)
{
    g_return_if_fail (loader != NULL);
}

gboolean
chicle_heif_loader_goto_next_frame (ChicleHeifLoader *loader)
{
    g_return_val_if_fail (loader != NULL, FALSE);

    return FALSE;
}
