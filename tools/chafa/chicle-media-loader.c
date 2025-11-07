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
#include "chicle-file-mapping.h"
#include "chicle-gif-loader.h"
#include "chicle-xwd-loader.h"
#include "chicle-jpeg-loader.h"
#include "chicle-media-loader.h"
#include "chicle-png-loader.h"
#include "chicle-qoi-loader.h"
#include "chicle-svg-loader.h"
#include "chicle-tiff-loader.h"
#include "chicle-webp-loader.h"
#include "chicle-avif-loader.h"
#include "chicle-jxl-loader.h"
#include "chicle-heif-loader.h"
#include "chicle-coregraphics-loader.h"

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
    LOADER_TYPE_JXL,
    LOADER_TYPE_COREGRAPHICS,
    LOADER_TYPE_HEIF,

    LOADER_TYPE_LAST
}
LoaderType;

typedef gpointer (NewFromMappingFunc) (gpointer, gint, gint);

static const struct
{
    const gchar * const name;
    void (*new_from_mapping) (void);
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
        (void (*)(void)) chicle_gif_loader_new_from_mapping,
        (gpointer (*)(gconstpointer)) NULL,
        (void (*)(gpointer)) chicle_gif_loader_destroy,
        (gboolean (*)(gpointer)) chicle_gif_loader_get_is_animation,
        (void (*)(gpointer)) chicle_gif_loader_goto_first_frame,
        (gboolean (*)(gpointer)) chicle_gif_loader_goto_next_frame,
        (gconstpointer (*) (gpointer, gpointer, gpointer, gpointer, gpointer)) chicle_gif_loader_get_frame_data,
        (gint (*) (gpointer)) chicle_gif_loader_get_frame_delay
    },
    [LOADER_TYPE_PNG] =
    {
        "PNG",
        (void (*)(void)) chicle_png_loader_new_from_mapping,
        (gpointer (*)(gconstpointer)) NULL,
        (void (*)(gpointer)) chicle_png_loader_destroy,
        (gboolean (*)(gpointer)) chicle_png_loader_get_is_animation,
        (void (*)(gpointer)) chicle_png_loader_goto_first_frame,
        (gboolean (*)(gpointer)) chicle_png_loader_goto_next_frame,
        (gconstpointer (*) (gpointer, gpointer, gpointer, gpointer, gpointer)) chicle_png_loader_get_frame_data,
        (gint (*) (gpointer)) chicle_png_loader_get_frame_delay
    },
    [LOADER_TYPE_XWD] =
    {
        "XWD",
        (void (*)(void)) chicle_xwd_loader_new_from_mapping,
        (gpointer (*)(gconstpointer)) NULL,
        (void (*)(gpointer)) chicle_xwd_loader_destroy,
        (gboolean (*)(gpointer)) chicle_xwd_loader_get_is_animation,
        (void (*)(gpointer)) chicle_xwd_loader_goto_first_frame,
        (gboolean (*)(gpointer)) chicle_xwd_loader_goto_next_frame,
        (gconstpointer (*) (gpointer, gpointer, gpointer, gpointer, gpointer)) chicle_xwd_loader_get_frame_data,
        (gint (*) (gpointer)) chicle_xwd_loader_get_frame_delay
    },
    [LOADER_TYPE_QOI] =
    {
        "QOI",
        (void (*)(void)) chicle_qoi_loader_new_from_mapping,
        (gpointer (*)(gconstpointer)) NULL,
        (void (*)(gpointer)) chicle_qoi_loader_destroy,
        (gboolean (*)(gpointer)) chicle_qoi_loader_get_is_animation,
        (void (*)(gpointer)) chicle_qoi_loader_goto_first_frame,
        (gboolean (*)(gpointer)) chicle_qoi_loader_goto_next_frame,
        (gconstpointer (*) (gpointer, gpointer, gpointer, gpointer, gpointer)) chicle_qoi_loader_get_frame_data,
        (gint (*) (gpointer)) chicle_qoi_loader_get_frame_delay
    },
#ifdef HAVE_JPEG
    [LOADER_TYPE_JPEG] =
    {
        "JPEG",
        (void (*)(void)) chicle_jpeg_loader_new_from_mapping,
        (gpointer (*)(gconstpointer)) NULL,
        (void (*)(gpointer)) chicle_jpeg_loader_destroy,
        (gboolean (*)(gpointer)) chicle_jpeg_loader_get_is_animation,
        (void (*)(gpointer)) chicle_jpeg_loader_goto_first_frame,
        (gboolean (*)(gpointer)) chicle_jpeg_loader_goto_next_frame,
        (gconstpointer (*) (gpointer, gpointer, gpointer, gpointer, gpointer)) chicle_jpeg_loader_get_frame_data,
        (gint (*) (gpointer)) chicle_jpeg_loader_get_frame_delay
    },
#endif
#ifdef HAVE_SVG
    [LOADER_TYPE_SVG] =
    {
        "SVG",
        (void (*)(void)) chicle_svg_loader_new_from_mapping,
        (gpointer (*)(gconstpointer)) NULL,
        (void (*)(gpointer)) chicle_svg_loader_destroy,
        (gboolean (*)(gpointer)) chicle_svg_loader_get_is_animation,
        (void (*)(gpointer)) chicle_svg_loader_goto_first_frame,
        (gboolean (*)(gpointer)) chicle_svg_loader_goto_next_frame,
        (gconstpointer (*) (gpointer, gpointer, gpointer, gpointer, gpointer)) chicle_svg_loader_get_frame_data,
        (gint (*) (gpointer)) chicle_svg_loader_get_frame_delay
    },
#endif
#ifdef HAVE_TIFF
    [LOADER_TYPE_TIFF] =
    {
        "TIFF",
        (void (*)(void)) chicle_tiff_loader_new_from_mapping,
        (gpointer (*)(gconstpointer)) NULL,
        (void (*)(gpointer)) chicle_tiff_loader_destroy,
        (gboolean (*)(gpointer)) chicle_tiff_loader_get_is_animation,
        (void (*)(gpointer)) chicle_tiff_loader_goto_first_frame,
        (gboolean (*)(gpointer)) chicle_tiff_loader_goto_next_frame,
        (gconstpointer (*) (gpointer, gpointer, gpointer, gpointer, gpointer)) chicle_tiff_loader_get_frame_data,
        (gint (*) (gpointer)) chicle_tiff_loader_get_frame_delay
    },
#endif
#ifdef HAVE_WEBP
    [LOADER_TYPE_WEBP] =
    {
        "WebP",
        (void (*)(void)) chicle_webp_loader_new_from_mapping,
        (gpointer (*)(gconstpointer)) NULL,
        (void (*)(gpointer)) chicle_webp_loader_destroy,
        (gboolean (*)(gpointer)) chicle_webp_loader_get_is_animation,
        (void (*)(gpointer)) chicle_webp_loader_goto_first_frame,
        (gboolean (*)(gpointer)) chicle_webp_loader_goto_next_frame,
        (gconstpointer (*) (gpointer, gpointer, gpointer, gpointer, gpointer)) chicle_webp_loader_get_frame_data,
        (gint (*) (gpointer)) chicle_webp_loader_get_frame_delay
    },
#endif
#ifdef HAVE_AVIF
    [LOADER_TYPE_AVIF] =
    {
        "AVIF",
        (void (*)(void)) chicle_avif_loader_new_from_mapping,
        (gpointer (*)(gconstpointer)) NULL,
        (void (*)(gpointer)) chicle_avif_loader_destroy,
        (gboolean (*)(gpointer)) chicle_avif_loader_get_is_animation,
        (void (*)(gpointer)) chicle_avif_loader_goto_first_frame,
        (gboolean (*)(gpointer)) chicle_avif_loader_goto_next_frame,
        (gconstpointer (*) (gpointer, gpointer, gpointer, gpointer, gpointer)) chicle_avif_loader_get_frame_data,
        (gint (*) (gpointer)) chicle_avif_loader_get_frame_delay
    },
#endif
#ifdef HAVE_JXL
    [LOADER_TYPE_JXL] =
    {
        "JXL",
        (void (*)(void)) chicle_jxl_loader_new_from_mapping,
        (gpointer (*)(gconstpointer)) NULL,
        (void (*)(gpointer)) chicle_jxl_loader_destroy,
        (gboolean (*)(gpointer)) chicle_jxl_loader_get_is_animation,
        (void (*)(gpointer)) chicle_jxl_loader_goto_first_frame,
        (gboolean (*)(gpointer)) chicle_jxl_loader_goto_next_frame,
        (gconstpointer (*) (gpointer, gpointer, gpointer, gpointer, gpointer)) chicle_jxl_loader_get_frame_data,
        (gint (*) (gpointer)) chicle_jxl_loader_get_frame_delay
    },
#endif
#ifdef HAVE_COREGRAPHICS
    [LOADER_TYPE_COREGRAPHICS] =
    {
        "CoreGraphics",
        (void (*)(void)) chicle_coregraphics_loader_new_from_mapping,
        (gpointer (*)(gconstpointer)) NULL,
        (void (*)(gpointer)) chicle_coregraphics_loader_destroy,
        (gboolean (*)(gpointer)) chicle_coregraphics_loader_get_is_animation,
        (void (*)(gpointer)) chicle_coregraphics_loader_goto_first_frame,
        (gboolean (*)(gpointer)) chicle_coregraphics_loader_goto_next_frame,
        (gconstpointer (*) (gpointer, gpointer, gpointer, gpointer, gpointer)) chicle_coregraphics_loader_get_frame_data,
        (gint (*) (gpointer)) chicle_coregraphics_loader_get_frame_delay
    },
#endif
#ifdef HAVE_HEIF
    /* Due to its complexity and broad format support, libheif should run last */
    [LOADER_TYPE_HEIF] =
    {
        "HEIF",
        (void (*)(void)) chicle_heif_loader_new_from_mapping,
        (gpointer (*)(gconstpointer)) NULL,
        (void (*)(gpointer)) chicle_heif_loader_destroy,
        (gboolean (*)(gpointer)) chicle_heif_loader_get_is_animation,
        (void (*)(gpointer)) chicle_heif_loader_goto_first_frame,
        (gboolean (*)(gpointer)) chicle_heif_loader_goto_next_frame,
        (gconstpointer (*) (gpointer, gpointer, gpointer, gpointer, gpointer)) chicle_heif_loader_get_frame_data,
        (gint (*) (gpointer)) chicle_heif_loader_get_frame_delay
    },
#endif
};

struct ChicleMediaLoader
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

ChicleMediaLoader *
chicle_media_loader_new (const gchar *path, gint target_width, gint target_height, GError **error)
{
    ChicleMediaLoader *loader;
    ChicleFileMapping *mapping = NULL;
    gboolean success = FALSE;
    gint i;

    g_return_val_if_fail (path != NULL, NULL);

    loader = g_new0 (ChicleMediaLoader, 1);
    mapping = chicle_file_mapping_new (path);

    if (!chicle_file_mapping_open_now (mapping, error))
        goto out;

    for (i = 0; i < LOADER_TYPE_LAST && !loader->loader; i++)
    {
        loader->loader_type = i;

        if (mapping && loader_vtable [i].new_from_mapping)
        {
            loader->loader = (*(NewFromMappingFunc *) loader_vtable [i].new_from_mapping)
                (mapping, target_width, target_height);
        }
        else if (loader_vtable [i].new_from_path)
        {
            loader->loader = loader_vtable [i].new_from_path (path);
            if (loader->loader)
                chicle_file_mapping_destroy (mapping);
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
            chicle_file_mapping_destroy (mapping);

        chicle_media_loader_destroy (loader);
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
chicle_media_loader_destroy (ChicleMediaLoader *loader)
{
    if (loader->loader)
    {
        loader_vtable [loader->loader_type].destroy (loader->loader);
        loader->loader = NULL;
    }

    g_free (loader);
}

gboolean
chicle_media_loader_get_is_animation (ChicleMediaLoader *loader)
{
    return loader_vtable [loader->loader_type].get_is_animation (loader->loader);
}

void
chicle_media_loader_goto_first_frame (ChicleMediaLoader *loader)
{
    loader_vtable [loader->loader_type].goto_first_frame (loader->loader);
}

gboolean
chicle_media_loader_goto_next_frame (ChicleMediaLoader *loader)
{
    return loader_vtable [loader->loader_type].goto_next_frame (loader->loader);
}

gconstpointer
chicle_media_loader_get_frame_data (ChicleMediaLoader *loader,
                                    ChafaPixelType *pixel_type_out,
                                    gint *width_out,
                                    gint *height_out,
                                    gint *rowstride_out)
{
    return loader_vtable [loader->loader_type].get_frame_data (loader->loader, pixel_type_out,
                                                               width_out, height_out, rowstride_out);
}

gint
chicle_media_loader_get_frame_delay (ChicleMediaLoader *loader)
{
    return loader_vtable [loader->loader_type].get_frame_delay (loader->loader);
}

gchar **
chicle_get_loader_names (void)
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
