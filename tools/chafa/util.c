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
#include <stdlib.h>
#include <string.h>

#include <chafa.h>
#include "util.h"

#define ROWSTRIDE_ALIGN 16

#define PAD_TO_N(p, n) (((p) + ((n) - 1)) & ~((unsigned) (n) - 1))
#define ROWSTRIDE_PAD(rowstride) (PAD_TO_N ((rowstride), (ROWSTRIDE_ALIGN)))

static void
transform (const guchar *src, gint src_pixstride, gint src_rowstride,
           guchar *dest, gint dest_pixstride, gint dest_rowstride,
           gint src_width, gint src_height, gint pixsize)
{
    const guchar *src_row = src;
    const guchar *src_row_end = src + src_pixstride * src_width;
    const guchar *src_end = src + src_rowstride * src_height;
    guchar *dest_row = dest;

    while (src_row != src_end)
    {
        src = src_row;
        dest = dest_row;

        while (src != src_row_end)
        {
            memcpy (dest, src, pixsize);
            src += src_pixstride;
            dest += dest_pixstride;
        }

        src_row += src_rowstride;
        src_row_end += src_rowstride;
        dest_row += dest_rowstride;
    }
}

RotationType
invert_rotation (RotationType rot)
{
    switch (rot)
    {
        case ROTATION_90:
            rot = ROTATION_270;
            break;
        case ROTATION_270:
            rot = ROTATION_90;
            break;
        default:
            break;
    }

    return rot;
}

void
rotate_image (guchar **src, guint *width, guint *height, guint *rowstride,
              guint n_channels, RotationType rot)
{
    gint src_width, src_height;
    gint src_pixstride, src_rowstride;
    guchar *dest, *dest_start;
    gint dest_width, dest_height;
    gint dest_pixstride, dest_rowstride, dest_trans_rowstride;

    g_assert (n_channels == 3 || n_channels == 4);

    if (rot <= ROTATION_NONE
        || rot == ROTATION_0
        || rot >= ROTATION_UNDEFINED)
        return;

    src_width = *width;
    src_height = *height;
    src_pixstride = n_channels;
    src_rowstride = *rowstride;

    switch (rot)
    {
        case ROTATION_90:
        case ROTATION_90_MIRROR:
        case ROTATION_270:
        case ROTATION_270_MIRROR:
            dest_width = src_height;
            dest_height = src_width;
            break;
        case ROTATION_0_MIRROR:
        case ROTATION_180:
        case ROTATION_180_MIRROR:
            dest_width = src_width;
            dest_height = src_height;
            break;
        default:
            g_assert_not_reached ();
    }

    dest_rowstride = ROWSTRIDE_PAD (dest_width * n_channels);
    dest = g_malloc (dest_rowstride * dest_height);

    switch (rot)
    {
        case ROTATION_0_MIRROR:
            dest_pixstride = -n_channels;
            dest_trans_rowstride = dest_rowstride;
            dest_start = dest + ((dest_width - 1) * n_channels);
            break;
        case ROTATION_90:
            dest_pixstride = dest_rowstride;
            dest_trans_rowstride = -n_channels;
            dest_start = dest + ((dest_width - 1) * n_channels);
            break;
        case ROTATION_90_MIRROR:
            dest_pixstride = -dest_rowstride;
            dest_trans_rowstride = -n_channels;
            dest_start = dest + ((dest_height - 1) * dest_rowstride) + ((dest_width - 1) * n_channels);
            break;
        case ROTATION_180:
            dest_pixstride = -n_channels;
            dest_trans_rowstride = -dest_rowstride;
            dest_start = dest + ((dest_height - 1) * dest_rowstride) + ((dest_width - 1) * n_channels);
            break;
        case ROTATION_180_MIRROR:
            dest_pixstride = n_channels;
            dest_trans_rowstride = -dest_rowstride;
            dest_start = dest + ((dest_height - 1) * dest_rowstride);
            break;
        case ROTATION_270:
            dest_pixstride = -dest_rowstride;
            dest_trans_rowstride = n_channels;
            dest_start = dest + ((dest_height - 1) * dest_rowstride);
            break;
        case ROTATION_270_MIRROR:
            dest_pixstride = dest_rowstride;
            dest_trans_rowstride = n_channels;
            dest_start = dest;
            break;
        default:
            g_assert_not_reached ();
    }

    transform (*src, src_pixstride, src_rowstride,
               dest_start, dest_pixstride, dest_trans_rowstride,
               src_width, src_height, n_channels);

    g_free (*src);

    *src = dest;
    *width = dest_width;
    *height = dest_height;
    *rowstride = dest_rowstride;
}
