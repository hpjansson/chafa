/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2022 Hans Petter Jansson
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
#include "file-mapping.h"
#include "gif-loader.h"
#include "im-loader.h"
#include "xwd-loader.h"
#include "jpeg-loader.h"
#include "media-loader.h"
#include "png-loader.h"

typedef enum
{
    LOADER_TYPE_GIF,
    LOADER_TYPE_PNG,
    LOADER_TYPE_XWD,
    LOADER_TYPE_JPEG,
    LOADER_TYPE_IMAGEMAGICK,

    LOADER_TYPE_LAST
}
LoaderType;

struct
{
    void (*destroy) (gpointer);
    gboolean (*get_is_animation) (gpointer);
    void (*goto_first_frame) (gpointer);
    gboolean (*goto_next_frame) (gpointer);
    gconstpointer (*get_frame_data) (gpointer, gpointer, gpointer, gpointer, gpointer);
    gint (*get_frame_delay) (gpointer);
}
loader_vtable [LOADER_TYPE_LAST] =
{
    [LOADER_TYPE_GIF] =
    {
        (void (*)(gpointer)) gif_loader_destroy,
        (gboolean (*)(gpointer)) gif_loader_get_is_animation,
        (void (*)(gpointer)) gif_loader_goto_first_frame,
        (gboolean (*)(gpointer)) gif_loader_goto_next_frame,
        (gconstpointer (*) (gpointer, gpointer, gpointer, gpointer, gpointer)) gif_loader_get_frame_data,
        (gint (*) (gpointer)) gif_loader_get_frame_delay
    },
    [LOADER_TYPE_PNG] =
    {
        (void (*)(gpointer)) png_loader_destroy,
        (gboolean (*)(gpointer)) png_loader_get_is_animation,
        (void (*)(gpointer)) png_loader_goto_first_frame,
        (gboolean (*)(gpointer)) png_loader_goto_next_frame,
        (gconstpointer (*) (gpointer, gpointer, gpointer, gpointer, gpointer)) png_loader_get_frame_data,
        (gint (*) (gpointer)) png_loader_get_frame_delay
    },
    [LOADER_TYPE_XWD] =
    {
        (void (*)(gpointer)) xwd_loader_destroy,
        (gboolean (*)(gpointer)) xwd_loader_get_is_animation,
        (void (*)(gpointer)) xwd_loader_goto_first_frame,
        (gboolean (*)(gpointer)) xwd_loader_goto_next_frame,
        (gconstpointer (*) (gpointer, gpointer, gpointer, gpointer, gpointer)) xwd_loader_get_frame_data,
        (gint (*) (gpointer)) xwd_loader_get_frame_delay
    },
    [LOADER_TYPE_JPEG] =
    {
        (void (*)(gpointer)) jpeg_loader_destroy,
        (gboolean (*)(gpointer)) jpeg_loader_get_is_animation,
        (void (*)(gpointer)) jpeg_loader_goto_first_frame,
        (gboolean (*)(gpointer)) jpeg_loader_goto_next_frame,
        (gconstpointer (*) (gpointer, gpointer, gpointer, gpointer, gpointer)) jpeg_loader_get_frame_data,
        (gint (*) (gpointer)) jpeg_loader_get_frame_delay
    },
    [LOADER_TYPE_IMAGEMAGICK] =
    {
        (void (*)(gpointer)) im_loader_destroy,
        (gboolean (*)(gpointer)) im_loader_get_is_animation,
        (void (*)(gpointer)) im_loader_goto_first_frame,
        (gboolean (*)(gpointer)) im_loader_goto_next_frame,
        (gconstpointer (*) (gpointer, gpointer, gpointer, gpointer, gpointer)) im_loader_get_frame_data,
        (gint (*) (gpointer)) im_loader_get_frame_delay
    }
};

struct MediaLoader
{
    LoaderType loader_type;
    gpointer loader;
};

MediaLoader *
media_loader_new (const gchar *path)
{
    MediaLoader *loader;
    FileMapping *mapping;
    gboolean success = FALSE;

    g_return_val_if_fail (path != NULL, NULL);

    loader = g_new0 (MediaLoader, 1);
    mapping = file_mapping_new (path);
    if (mapping)
    {
        /* GIF */

        loader->loader_type = LOADER_TYPE_GIF;
        loader->loader = gif_loader_new_from_mapping (mapping);

        /* PNG */

        if (!loader->loader)
        {
            loader->loader_type = LOADER_TYPE_PNG;
            loader->loader = png_loader_new_from_mapping (mapping);
        }

        /* XWD */

        if (!loader->loader)
        {
            loader->loader_type = LOADER_TYPE_XWD;
            loader->loader = xwd_loader_new_from_mapping (mapping);
        }

        /* JPEG */

        if (!loader->loader)
        {
            loader->loader_type = LOADER_TYPE_JPEG;
            loader->loader = jpeg_loader_new_from_mapping (mapping);
        }

        /* Format loader will take ownership of mapping. On failure, we
         * have to destroy it manually. */

        if (!loader->loader)
        {
            file_mapping_destroy (mapping);
            mapping = NULL;
        }
    }

    /* Any via ImageMagick */

    if (!loader->loader)
    {
        loader->loader_type = LOADER_TYPE_IMAGEMAGICK;
        loader->loader = im_loader_new (path);
    }

    if (!loader->loader)
    {
        loader->loader_type = LOADER_TYPE_LAST;
        goto out;
    }

    success = TRUE;

out:
    if (!success)
    {
        media_loader_destroy (loader);
        loader = NULL;
    }

    return loader;
}

void
media_loader_destroy (MediaLoader *loader)
{
    if (loader->loader)
    {
        loader_vtable [loader->loader_type].destroy (loader->loader);
        loader->loader = NULL;
    }

    g_free (loader);
}

gboolean
media_loader_get_is_animation (MediaLoader *loader)
{
    return loader_vtable [loader->loader_type].get_is_animation (loader->loader);
}

void
media_loader_goto_first_frame (MediaLoader *loader)
{
    loader_vtable [loader->loader_type].goto_first_frame (loader->loader);
}

gboolean
media_loader_goto_next_frame (MediaLoader *loader)
{
    return loader_vtable [loader->loader_type].goto_next_frame (loader->loader);
}

gconstpointer
media_loader_get_frame_data (MediaLoader *loader, ChafaPixelType *pixel_type_out,
                             gint *width_out, gint *height_out, gint *rowstride_out)
{
    return loader_vtable [loader->loader_type].get_frame_data (loader->loader, pixel_type_out,
                                                               width_out, height_out, rowstride_out);
}

gint
media_loader_get_frame_delay (MediaLoader *loader)
{
    return loader_vtable [loader->loader_type].get_frame_delay (loader->loader);
}
