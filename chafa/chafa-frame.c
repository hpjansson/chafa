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
#include "chafa.h"
#include "internal/chafa-private.h"

/**
 * SECTION:chafa-frame
 * @title: ChafaFrame
 * @short_description: A raster image frame that can be added to a ChafaImage
 *
 * A #ChafaFrame contains the specific of a single frame of image data. It can
 * be added to a #ChafaImage.
 **/

static ChafaFrame *
new_frame (gpointer data,
           ChafaPixelType pixel_type,
           gint width, gint height, gint rowstride,
           gboolean data_is_owned)
{
    ChafaFrame *frame;

    frame = g_new0 (ChafaFrame, 1);
    frame->refs = 1;

    frame->pixel_type = pixel_type;
    frame->width = width;
    frame->height = height;
    frame->rowstride = rowstride;
    frame->data = data;
    frame->data_is_owned = data_is_owned;

    return frame;
}

/**
 * chafa_frame_new:
 * @data: Pointer to an image data buffer to copy from
 * @pixel_type: The #ChafaPixelType of the source data
 * @width: Width of the image, in pixels
 * @height: Height of the image, in pixels
 * @rowstride: Number of bytes to advance from the start of one row to the next
 *
 * Creates a new #ChafaFrame containing a copy of the image data pointed to
 * by @data.
 *
 * Returns: The new frame
 *
 * Since: 1.14
 **/
ChafaFrame *
chafa_frame_new (gconstpointer data,
                 ChafaPixelType pixel_type,
                 gint width, gint height, gint rowstride)
{
    gpointer owned_data;

    owned_data = g_malloc (height * rowstride);
    memcpy (owned_data, data, height * rowstride);
    return new_frame (owned_data, pixel_type, width, height, rowstride, TRUE);
}

/**
 * chafa_frame_new_steal:
 * @data: Pointer to an image data buffer to assign
 * @pixel_type: The #ChafaPixelType of the buffer
 * @width: Width of the image, in pixels
 * @height: Height of the image, in pixels
 * @rowstride: Number of bytes to advance from the start of one row to the next
 *
 * Creates a new #ChafaFrame, which takes ownership of the @data buffer. The
 * buffer will be freed with #g_free() when the frame's reference count drops
 * to zero.
 *
 * Returns: The new frame
 *
 * Since: 1.14
 **/
ChafaFrame *
chafa_frame_new_steal (gpointer data,
                       ChafaPixelType pixel_type,
                       gint width, gint height, gint rowstride)
{
    return new_frame (data, pixel_type, width, height, rowstride, TRUE);
}

/**
 * chafa_frame_new_borrow:
 * @data: Pointer to an image data buffer to assign
 * @pixel_type: The #ChafaPixelType of the buffer
 * @width: Width of the image, in pixels
 * @height: Height of the image, in pixels
 * @rowstride: Number of bytes to advance from the start of one row to the next
 *
 * Creates a new #ChafaFrame embedding the @data pointer. It's the caller's
 * responsibility to ensure the pointer remains valid for the lifetime of
 * the frame. The frame will not free the buffer when its reference count drops
 * to zero.
 *
 * THIS IS DANGEROUS API which should only be used when the life cycle of the
 * frame is short, stealing the buffer is impossible, and copying would cause
 * unacceptable performance degradation.
 *
 * Use #chafa_frame_new() instead.
 *
 * Returns: The new frame
 *
 * Since: 1.14
 **/
ChafaFrame *
chafa_frame_new_borrow (gpointer data,
                        ChafaPixelType pixel_type,
                        gint width, gint height, gint rowstride)
{
    return new_frame (data, pixel_type, width, height, rowstride, FALSE);
}

/**
 * chafa_frame_ref:
 * @frame: Frame to add a reference to
 *
 * Adds a reference to @frame.
 *
 * Since: 1.14
 **/
void
chafa_frame_ref (ChafaFrame *frame)
{
    gint refs;

    g_return_if_fail (frame != NULL);
    refs = g_atomic_int_get (&frame->refs);
    g_return_if_fail (refs > 0);

    g_atomic_int_inc (&frame->refs);
}

/**
 * chafa_frame_unref:
 * @frame: Frame to remove a reference from
 *
 * Removes a reference from @frame. When the reference count drops to zero,
 * the frame is freed and can no longer be used.
 *
 * Since: 1.14
 **/
void
chafa_frame_unref (ChafaFrame *frame)
{
    gint refs;

    g_return_if_fail (frame != NULL);
    refs = g_atomic_int_get (&frame->refs);
    g_return_if_fail (refs > 0);

    if (g_atomic_int_dec_and_test (&frame->refs))
    {
        if (frame->data_is_owned)
            g_free (frame->data);
        g_free (frame);
    }
}
