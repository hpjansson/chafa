/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef __CHICLE_IMAGE_SIZE_H__
#define __CHICLE_IMAGE_SIZE_H__

#include <glib.h>

static inline gboolean
chicle_checked_image_buffer_size (guint64 width, guint64 height, guint64 n_channels,
                                  guint64 max_size, gsize *size_out)
{
    guint64 size;

    if (width < 1 || height < 1 || n_channels < 1)
        return FALSE;
    if (width > G_MAXSIZE / height)
        return FALSE;

    size = width * height;
    if (size > G_MAXSIZE / n_channels)
        return FALSE;

    size *= n_channels;
    if (size > max_size)
        return FALSE;

    if (size_out)
        *size_out = (gsize) size;

    return TRUE;
}

#endif /* __CHICLE_IMAGE_SIZE_H__ */
