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

#ifdef HAVE_WAND_MAGICKWAND_H
# include <wand/MagickWand.h>
#else /* HAVE_MAGICKWAND_MAGICKWAND_H */
# include <MagickWand/MagickWand.h>
#endif

#include <chafa.h>
#include "im-loader.h"

struct ImLoader
{
    MagickWand *wand;
    gpointer current_frame_data;
};

static void
clear_current_frame_data (ImLoader *loader)
{
    g_free (loader->current_frame_data);
    loader->current_frame_data = NULL;
}

static void
auto_orient_image (MagickWand *image)
{
#ifdef HAVE_MAGICK_AUTO_ORIENT_IMAGE
    MagickAutoOrientImage (image);
#else
    PixelWand *pwand = NULL;

    switch (MagickGetImageOrientation (image))
    {
        case UndefinedOrientation:
        case TopLeftOrientation:
        default:
            break;
        case TopRightOrientation:
            MagickFlopImage (image);
            break;
        case BottomRightOrientation:
            pwand = NewPixelWand ();
            MagickRotateImage (image, pwand, 180.0);
            break;
        case BottomLeftOrientation:
            MagickFlipImage (image);
            break;
        case LeftTopOrientation:
            MagickTransposeImage (image);
            break;
        case RightTopOrientation:
            pwand = NewPixelWand ();
            MagickRotateImage (image, pwand, 90.0);
            break;
        case RightBottomOrientation:
            MagickTransverseImage (image);
            break;
        case LeftBottomOrientation:
            pwand = NewPixelWand ();
            MagickRotateImage (image, pwand, 270.0);
            break;
    }

    if (pwand)
        DestroyPixelWand (pwand);

    MagickSetImageOrientation (image, TopLeftOrientation);
#endif
}

static gint active_count = 0;

static void
active_count_inc (void)
{
    if (active_count == 0)
    {
        MagickWandGenesis ();
    }

    active_count++;
}

static void
active_count_dec (void)
{
    active_count--;

    if (active_count == 0)
    {
#if 0
        /* FIXME: Do this once at program exit only */
        MagickWandTerminus ();
#endif
    }
}

ImLoader *
im_loader_new (const gchar *path)
{
    PixelWand *color;
    ImLoader *loader;
    gboolean is_animation;
    gboolean success = FALSE;

    g_return_val_if_fail (path != NULL, NULL);

    active_count_inc ();

    loader = g_new0 (ImLoader, 1);

    loader->wand = NewMagickWand ();

    color = NewPixelWand ();
    PixelSetColor (color, "none");
    MagickSetBackgroundColor (loader->wand, color);
    DestroyPixelWand (color);

    if (MagickReadImage (loader->wand, path) < 1)
    {
        gchar *error_str = NULL;
        ExceptionType severity;
        gchar *try_path;
        gint r;

        error_str = MagickGetException (loader->wand, &severity);

        /* Try backup strategy for XWD. It's a file type we want to support
         * due to the fun implications with Xvfb etc. The paths in use
         * tend to have no extension, and the file magic isn't very definite,
         * so ImageMagick doesn't know what to do on its own. */
        try_path = g_strdup_printf ("XWD:%s", path);
        r = MagickReadImage (loader->wand, try_path);
        g_free (try_path);

        if (r < 1)
        {
#if 0
            if (!quiet)
                g_printerr ("%s: Error loading '%s': %s\n",
                            options.executable_name,
                            path,
                            error_str);
#endif
            MagickRelinquishMemory (error_str);
            goto out;
        }
    }

    is_animation = MagickGetNumberImages (loader->wand) > 1 ? TRUE : FALSE;

    if (is_animation)
    {
        MagickWand *wand2 = MagickCoalesceImages (loader->wand);
        loader->wand = DestroyMagickWand (loader->wand);
        loader->wand = wand2;
    }

    MagickResetIterator (loader->wand);
    MagickNextImage (loader->wand); /* ? */

    success = TRUE;

out:
    if (!success)
    {
        im_loader_destroy (loader);
        loader = NULL;
    }

    return loader;
}

void
im_loader_destroy (ImLoader *loader)
{
    clear_current_frame_data (loader);

    if (loader->wand)
    {
        DestroyMagickWand (loader->wand);
        loader->wand = NULL;
    }

    g_free (loader);

    active_count_dec ();
}

gboolean
im_loader_get_is_animation (ImLoader *loader)
{
    g_return_val_if_fail (loader != NULL, FALSE);

    return MagickGetNumberImages (loader->wand) > 1 ? TRUE : FALSE;
}

gconstpointer
im_loader_get_frame_data (ImLoader *loader, ChafaPixelType *pixel_type_out,
                          gint *width_out, gint *height_out, gint *rowstride_out)
{
    g_return_val_if_fail (loader != NULL, NULL);

    auto_orient_image (loader->wand);

    *width_out = MagickGetImageWidth (loader->wand);
    *height_out = MagickGetImageHeight (loader->wand);
    *rowstride_out = *width_out * 4;

    if (!loader->current_frame_data)
    {
        loader->current_frame_data = g_malloc (*height_out * *rowstride_out);
        MagickExportImagePixels (loader->wand,
                                 0, 0,
                                 *width_out, *height_out,
                                 "RGBA",
                                 CharPixel,
                                 (void *) loader->current_frame_data);
    }

    *pixel_type_out = CHAFA_PIXEL_RGBA8_UNASSOCIATED;

    return loader->current_frame_data;
}

gint
im_loader_get_frame_delay (ImLoader *loader)
{
    gint delay_ms;

    g_return_val_if_fail (loader != NULL, 0);

    delay_ms = MagickGetImageDelay (loader->wand) * 10;
    if (delay_ms == 0)
        delay_ms = 50;

    return delay_ms;
}

void
im_loader_goto_first_frame (ImLoader *loader)
{
    g_return_if_fail (loader != NULL);

    clear_current_frame_data (loader);
    MagickResetIterator (loader->wand);
    MagickNextImage (loader->wand); /* ? */
}

gboolean
im_loader_goto_next_frame (ImLoader *loader)
{
    g_return_val_if_fail (loader != NULL, FALSE);

    clear_current_frame_data (loader);
    return MagickNextImage (loader->wand) ? TRUE : FALSE;
}
