/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2023 Hans Petter Jansson
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

#include <chafa.h>
#include "qoi-loader.h"

#define QOI_IMPLEMENTATION
#define QOI_NO_STDIO
#include "qoi.h"

#define BYTES_PER_PIXEL 4

struct QoiLoader
{
    FileMapping *mapping;
    const guint8 *file_data;
    size_t file_data_len;
    gpointer frame_data;
    gint width, height;
};

static QoiLoader *
qoi_loader_new (void)
{
    return g_new0 (QoiLoader, 1);
}

QoiLoader *
qoi_loader_new_from_mapping (FileMapping *mapping)
{
    QoiLoader *loader = NULL;
    gboolean success = FALSE;
    unsigned char *frame_data = NULL;
    qoi_desc desc;

    g_return_val_if_fail (mapping != NULL, NULL);

    if (!file_mapping_has_magic (mapping, 0, "qoif", 4))
        goto out;

    loader = qoi_loader_new ();
    loader->mapping = mapping;

    loader->file_data = file_mapping_get_data (loader->mapping, &loader->file_data_len);
    if (!loader->file_data)
        goto out;

    /* Decodes to RGBA8 */
    frame_data = qoi_decode (loader->file_data, loader->file_data_len, &desc, BYTES_PER_PIXEL);
    if (!frame_data)
        goto out;

    if (desc.width < 1 || desc.width >= (1 << 16)
        || desc.height < 1 || desc.height >= (1 << 16))
        goto out;

    g_printerr ("desc.width=%d desc.height=%d\n",
                desc.width, desc.height);

    loader->frame_data = frame_data;
    loader->width = desc.width;
    loader->height = desc.height;

    success = TRUE;

out:
    if (!success)
    {
        if (loader)
        {
            g_free (loader);
            loader = NULL;
        }

        if (frame_data)
            free (frame_data);
    }

    return loader;
}

void
qoi_loader_destroy (QoiLoader *loader)
{
    if (loader->mapping)
        file_mapping_destroy (loader->mapping);

    if (loader->frame_data)
        free (loader->frame_data);

    g_free (loader);
}

gboolean
qoi_loader_get_is_animation (QoiLoader *loader)
{
    g_return_val_if_fail (loader != NULL, 0);

    return FALSE;
}

gconstpointer
qoi_loader_get_frame_data (QoiLoader *loader, ChafaPixelType *pixel_type_out,
                           gint *width_out, gint *height_out, gint *rowstride_out)
{
    g_return_val_if_fail (loader != NULL, NULL);

    if (pixel_type_out)
        *pixel_type_out = CHAFA_PIXEL_RGBA8_UNASSOCIATED;
    if (width_out)
        *width_out = loader->width;
    if (height_out)
        *height_out = loader->height;
    if (rowstride_out)
        *rowstride_out = loader->width * BYTES_PER_PIXEL;

    return loader->frame_data;
}

gint
qoi_loader_get_frame_delay (QoiLoader *loader)
{
    g_return_val_if_fail (loader != NULL, 0);

    return 0;
}

void
qoi_loader_goto_first_frame (QoiLoader *loader)
{
    g_return_if_fail (loader != NULL);
}

gboolean
qoi_loader_goto_next_frame (QoiLoader *loader)
{
    g_return_val_if_fail (loader != NULL, FALSE);

    return FALSE;
}
