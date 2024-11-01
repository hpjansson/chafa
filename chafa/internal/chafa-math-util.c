/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2021-2024 Hans Petter Jansson
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

#include <math.h>
#include "internal/chafa-private.h"
#include "internal/chafa-math-util.h"

static gint
align_dim (ChafaAlign align, gint src_size, gint dest_size)
{
    gint ofs;

    g_return_val_if_fail (src_size <= dest_size, 0);

    switch (align)
    {
        case CHAFA_ALIGN_START:
            ofs = 0;
            break;

        case CHAFA_ALIGN_CENTER:
            ofs = (dest_size - src_size) / 2;
            break;

        case CHAFA_ALIGN_END:
            ofs = dest_size - src_size;
            break;

        default:
            g_assert_not_reached ();
    }

    return ofs;
}

void
chafa_tuck_and_align (gint src_width, gint src_height,
                      gint dest_width, gint dest_height,
                      ChafaAlign halign, ChafaAlign valign,
                      ChafaTuck tuck,
                      gint *ofs_x_out, gint *ofs_y_out,
                      gint *width_out, gint *height_out)
{
    gfloat ratio [2];

    switch (tuck)
    {
        case CHAFA_TUCK_STRETCH:
            *ofs_x_out = 0;
            *ofs_y_out = 0;
            *width_out = dest_width;
            *height_out = dest_height;
            break;

        case CHAFA_TUCK_SHRINK_TO_FIT:
            if (src_width <= dest_width && src_height <= dest_height)
            {
                /* Image fits entirely in dest. Do alignment only, no scaling. */
                *width_out = src_width;
                *height_out = src_height;
                break;
            }

            /* Fall through */

        case CHAFA_TUCK_FIT:
            ratio [0] = (gfloat) dest_width / (gfloat) src_width;
            ratio [1] = (gfloat) dest_height / (gfloat) src_height;

            *width_out = ceilf (src_width * MIN (ratio [0], ratio [1]));
            *height_out = ceilf (src_height * MIN (ratio [0], ratio [1]));
            break;

        default:
            g_assert_not_reached ();
    }

    /* Never exceed the dest size */
    *width_out = MIN (*width_out, dest_width);
    *height_out = MIN (*height_out, dest_height);

    *ofs_x_out = align_dim (halign, *width_out, dest_width);
    *ofs_y_out = align_dim (valign, *height_out, dest_height);

#if 0
    g_printerr ("src=(%dx%d) dest=(%dx%d) out=(%dx%d) @ (%d,%d)\n",
                src_width, src_height,
                dest_width, dest_height,
                *width_out, *height_out,
                *ofs_x_out, *ofs_y_out);
#endif
}

