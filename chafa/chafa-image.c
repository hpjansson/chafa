/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2024 Hans Petter Jansson
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
#include "chafa.h"
#include "internal/chafa-private.h"

/**
 * SECTION:chafa-image
 * @title: ChafaImage
 * @short_description: An image to be placed on a #ChafaCanvas
 *
 * A #ChafaImage represents a raster image for placement on a #ChafaCanvas. It
 * can currently hold a single #ChafaFrame.
 *
 * To place an image on a canvas, it must first be assigned to a #ChafaPlacement.
 **/

/**
 * chafa_image_new:
 *
 * Creates a new #ChafaImage. The image is initially transparent and dimensionless.
 *
 * Returns: The new image
 *
 * Since: 1.14
 **/
ChafaImage *
chafa_image_new (void)
{
    ChafaImage *image;

    image = g_new0 (ChafaImage, 1);
    image->refs = 1;

    return image;
}

/**
 * chafa_image_ref:
 * @image: Image to add a reference to
 *
 * Adds a reference to @image.
 *
 * Since: 1.14
 **/
void
chafa_image_ref (ChafaImage *image)
{
    gint refs;

    g_return_if_fail (image != NULL);
    refs = g_atomic_int_get (&image->refs);
    g_return_if_fail (refs > 0);

    g_atomic_int_inc (&image->refs);
}

/**
 * chafa_image_unref:
 * @image: Image to remove a reference from
 *
 * Removes a reference from @image. When the reference count drops to zero,
 * the image is freed and can no longer be used.
 *
 * Since: 1.14
 **/
void
chafa_image_unref (ChafaImage *image)
{
    gint refs;

    g_return_if_fail (image != NULL);
    refs = g_atomic_int_get (&image->refs);
    g_return_if_fail (refs > 0);

    if (g_atomic_int_dec_and_test (&image->refs))
    {
        if (image->frame)
            chafa_frame_unref (image->frame);
        g_free (image);
    }
}

/**
 * chafa_image_set_frame:
 * @image: Image to assign frame to
 * @frame: Frame to be assigned to image
 *
 * Assigns @frame as the content for @image. The image will keep its own
 * reference to the frame.
 *
 * Since: 1.14
 **/
void
chafa_image_set_frame (ChafaImage *image, ChafaFrame *frame)
{
    g_return_if_fail (image != NULL);

    if (frame)
        chafa_frame_ref (frame);
    if (image->frame)
        chafa_frame_unref (image->frame);

    image->frame = frame;
}
