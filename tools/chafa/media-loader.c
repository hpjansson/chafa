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
#include "xwd-loader.h"
#include "jpeg-loader.h"
#include "media-loader.h"
#include "png-loader.h"
#include "qoi-loader.h"
#include "svg-loader.h"
#include "tiff-loader.h"
#include "webp-loader.h"
#include "avif-loader.h"

typedef enum
{
    LOADER_TYPE_GIF,
    LOADER_TYPE_PNG,
    LOADER_TYPE_XWD,
    LOADER_TYPE_QOI,
    LOADER_TYPE_JPEG,
    LOADER_TYPE_TIFF,
    LOADER_TYPE_WEBP,
    LOADER_TYPE_AVIF,
    LOADER_TYPE_SVG,

    LOADER_TYPE_LAST
}
LoaderType;

static const struct
{
    const gchar * const name;
    gpointer (*new_from_mapping) (gpointer);
    gpointer (*new_from_path) (gconstpointer);
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
        "GIF",
        (gpointer (*)(gpointer)) gif_loader_new_from_mapping,
        (gpointer (*)(gconstpointer)) NULL,
        (void (*)(gpointer)) gif_loader_destroy,
        (gboolean (*)(gpointer)) gif_loader_get_is_animation,
        (void (*)(gpointer)) gif_loader_goto_first_frame,
        (gboolean (*)(gpointer)) gif_loader_goto_next_frame,
        (gconstpointer (*) (gpointer, gpointer, gpointer, gpointer, gpointer)) gif_loader_get_frame_data,
        (gint (*) (gpointer)) gif_loader_get_frame_delay
    },
    [LOADER_TYPE_PNG] =
    {
        "PNG",
        (gpointer (*)(gpointer)) png_loader_new_from_mapping,
        (gpointer (*)(gconstpointer)) NULL,
        (void (*)(gpointer)) png_loader_destroy,
        (gboolean (*)(gpointer)) png_loader_get_is_animation,
        (void (*)(gpointer)) png_loader_goto_first_frame,
        (gboolean (*)(gpointer)) png_loader_goto_next_frame,
        (gconstpointer (*) (gpointer, gpointer, gpointer, gpointer, gpointer)) png_loader_get_frame_data,
        (gint (*) (gpointer)) png_loader_get_frame_delay
    },
    [LOADER_TYPE_XWD] =
    {
        "XWD",
        (gpointer (*)(gpointer)) xwd_loader_new_from_mapping,
        (gpointer (*)(gconstpointer)) NULL,
        (void (*)(gpointer)) xwd_loader_destroy,
        (gboolean (*)(gpointer)) xwd_loader_get_is_animation,
        (void (*)(gpointer)) xwd_loader_goto_first_frame,
        (gboolean (*)(gpointer)) xwd_loader_goto_next_frame,
        (gconstpointer (*) (gpointer, gpointer, gpointer, gpointer, gpointer)) xwd_loader_get_frame_data,
        (gint (*) (gpointer)) xwd_loader_get_frame_delay
    },
    [LOADER_TYPE_QOI] =
    {
        "QOI",
        (gpointer (*)(gpointer)) qoi_loader_new_from_mapping,
        (gpointer (*)(gconstpointer)) NULL,
        (void (*)(gpointer)) qoi_loader_destroy,
        (gboolean (*)(gpointer)) qoi_loader_get_is_animation,
        (void (*)(gpointer)) qoi_loader_goto_first_frame,
        (gboolean (*)(gpointer)) qoi_loader_goto_next_frame,
        (gconstpointer (*) (gpointer, gpointer, gpointer, gpointer, gpointer)) qoi_loader_get_frame_data,
        (gint (*) (gpointer)) qoi_loader_get_frame_delay
    },
#ifdef HAVE_JPEG
    [LOADER_TYPE_JPEG] =
    {
        "JPEG",
        (gpointer (*)(gpointer)) jpeg_loader_new_from_mapping,
        (gpointer (*)(gconstpointer)) NULL,
        (void (*)(gpointer)) jpeg_loader_destroy,
        (gboolean (*)(gpointer)) jpeg_loader_get_is_animation,
        (void (*)(gpointer)) jpeg_loader_goto_first_frame,
        (gboolean (*)(gpointer)) jpeg_loader_goto_next_frame,
        (gconstpointer (*) (gpointer, gpointer, gpointer, gpointer, gpointer)) jpeg_loader_get_frame_data,
        (gint (*) (gpointer)) jpeg_loader_get_frame_delay
    },
#endif
#ifdef HAVE_SVG
    [LOADER_TYPE_SVG] =
    {
        "SVG",
        (gpointer (*)(gpointer)) svg_loader_new_from_mapping,
        (gpointer (*)(gconstpointer)) NULL,
        (void (*)(gpointer)) svg_loader_destroy,
        (gboolean (*)(gpointer)) svg_loader_get_is_animation,
        (void (*)(gpointer)) svg_loader_goto_first_frame,
        (gboolean (*)(gpointer)) svg_loader_goto_next_frame,
        (gconstpointer (*) (gpointer, gpointer, gpointer, gpointer, gpointer)) svg_loader_get_frame_data,
        (gint (*) (gpointer)) svg_loader_get_frame_delay
    },
#endif
#ifdef HAVE_TIFF
    [LOADER_TYPE_TIFF] =
    {
        "TIFF",
        (gpointer (*)(gpointer)) tiff_loader_new_from_mapping,
        (gpointer (*)(gconstpointer)) NULL,
        (void (*)(gpointer)) tiff_loader_destroy,
        (gboolean (*)(gpointer)) tiff_loader_get_is_animation,
        (void (*)(gpointer)) tiff_loader_goto_first_frame,
        (gboolean (*)(gpointer)) tiff_loader_goto_next_frame,
        (gconstpointer (*) (gpointer, gpointer, gpointer, gpointer, gpointer)) tiff_loader_get_frame_data,
        (gint (*) (gpointer)) tiff_loader_get_frame_delay
    },
#endif
#ifdef HAVE_WEBP
    [LOADER_TYPE_WEBP] =
    {
        "WebP",
        (gpointer (*)(gpointer)) webp_loader_new_from_mapping,
        (gpointer (*)(gconstpointer)) NULL,
        (void (*)(gpointer)) webp_loader_destroy,
        (gboolean (*)(gpointer)) webp_loader_get_is_animation,
        (void (*)(gpointer)) webp_loader_goto_first_frame,
        (gboolean (*)(gpointer)) webp_loader_goto_next_frame,
        (gconstpointer (*) (gpointer, gpointer, gpointer, gpointer, gpointer)) webp_loader_get_frame_data,
        (gint (*) (gpointer)) webp_loader_get_frame_delay
    },
#endif
#ifdef HAVE_AVIF
    [LOADER_TYPE_AVIF] =
    {
        "AVIF",
        (gpointer (*)(gpointer)) avif_loader_new_from_mapping,
        (gpointer (*)(gconstpointer)) NULL,
        (void (*)(gpointer)) avif_loader_destroy,
        (gboolean (*)(gpointer)) avif_loader_get_is_animation,
        (void (*)(gpointer)) avif_loader_goto_first_frame,
        (gboolean (*)(gpointer)) avif_loader_goto_next_frame,
        (gconstpointer (*) (gpointer, gpointer, gpointer, gpointer, gpointer)) avif_loader_get_frame_data,
        (gint (*) (gpointer)) avif_loader_get_frame_delay
    },
#endif
};

struct MediaLoader
{
    LoaderType loader_type;
    gpointer loader;
};

static int
ascii_strcasecmp_ptrs (const void *a, const void *b)
{
    gchar * const *sa = a, * const *sb = b;

    return g_ascii_strcasecmp (*sa, *sb);
}

MediaLoader *
media_loader_new (const gchar *path, GError **error)
{
    MediaLoader *loader;
    FileMapping *mapping = NULL;
    gboolean success = FALSE;
    gint i;

    g_return_val_if_fail (path != NULL, NULL);

    loader = g_new0 (MediaLoader, 1);
    mapping = file_mapping_new (path);

    if (!file_mapping_open_now (mapping, error))
        goto out;

    for (i = 0; i < LOADER_TYPE_LAST && !loader->loader; i++)
    {
        loader->loader_type = i;

        if (mapping && loader_vtable [i].new_from_mapping)
        {
            loader->loader = loader_vtable [i].new_from_mapping (mapping);
        }
        else if (loader_vtable [i].new_from_path)
        {
            loader->loader = loader_vtable [i].new_from_path (path);
            if (loader->loader)
                file_mapping_destroy (mapping);
        }

        if (loader->loader)
        {
            /* Mapping was either transferred to the loader or destroyed by us above */
            mapping = NULL;
            break;
        }
    }

    if (!loader->loader)
        goto out;

    success = TRUE;

out:
    if (!success)
    {
        if (mapping)
            file_mapping_destroy (mapping);

        media_loader_destroy (loader);
        loader = NULL;

        if (error && !*error)
        {
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                         "Unknown file format");
        }
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

gchar **
get_loader_names (void)
{
    gchar **strv;
    gint i, j;

    strv = g_new0 (gchar *, LOADER_TYPE_LAST + 1);

    for (i = 0, j = 0; i < LOADER_TYPE_LAST; i++)
    {
        if (loader_vtable [i].name == NULL)
            continue;

        strv [j++] = g_strdup (loader_vtable [i].name);
    }

    qsort (strv, j, sizeof (gchar *), ascii_strcasecmp_ptrs);

    return strv;
}
