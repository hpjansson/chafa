/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018 Hans Petter Jansson
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

#include "chafa/chafa.h"
#include "chafa/chafa-private.h"

/**
 * SECTION:chafa-util
 * @title: Miscellaneous
 * @short_description: Potentially useful functions
 *
 **/

/**
 * chafa_fit_aspect:
 * @src_width: Width of source
 * @src_height: Height of source
 * @dest_width_inout: Inout location for width of destination
 * @dest_height_inout: Inout location for height of destination
 * @font_ratio: Target font's width to height ratio
 * @zoom: %TRUE to upscale image to fit maximum dimenstions, %FALSE otherwise
 * @stretch: %TRUE to ignore aspect of source, %FALSE otherwise
 *
 * Calculates an optimal geometry for a #ChafaCanvas given the width and
 * height of an input image, maximum width and height of the canvas, font
 * ratio, zoom and stretch preferences.
 *
 * @src_width and @src_height must both be zero or greater.
 *
 * @dest_width_inout and @dest_height_inout must point to integers
 * containing the maximum dimensions of the canvas in character cells.
 * These will be replaced by the calculated values, which may be zero if
 * one of the input dimensions is zero.
 *
 * @font_ratio is the font's width divided by its height. 0.5 is a typical
 * value.
 **/
void
chafa_calc_canvas_geometry (gint src_width,
                            gint src_height,
                            gint *dest_width_inout,
                            gint *dest_height_inout,
                            gfloat font_ratio,
                            gboolean zoom,
                            gboolean stretch)
{
    gint dest_width = -1, dest_height = -1;

    g_return_if_fail (src_width >= 0);
    g_return_if_fail (src_height >= 0);

    if (dest_width_inout)
        dest_width = *dest_width_inout;
    if (dest_height_inout)
        dest_height = *dest_height_inout;

    if (src_width < 1 || src_height < 1
        || dest_width < 1 || dest_height < 1)
    {
        *dest_width_inout = 0;
        *dest_height_inout = 0;
        return;
    }

    if (!zoom)
    {
        dest_width = MIN (dest_width, src_width);
        dest_height = MIN (dest_height, src_height);
    }

    if (!stretch || dest_width < 0 || dest_height < 0)
    {
        gdouble src_aspect;
        gdouble dest_aspect;

        src_aspect = src_width / (gdouble) src_height;
        dest_aspect = (dest_width / (gdouble) dest_height) * font_ratio;

        if (dest_width < 1)
        {
            dest_width = dest_height * (src_aspect / font_ratio) + 0.5;
        }
        else if (dest_height < 1)
        {
            dest_height = (dest_width / src_aspect) * font_ratio + 0.5;
        }
        else if (src_aspect > dest_aspect)
        {
            dest_height = dest_width * (font_ratio / src_aspect);
        }
        else
        {
            dest_width = dest_height * (src_aspect / font_ratio);
        }
    }

    dest_width = MAX (dest_width, 1);
    dest_height = MAX (dest_height, 1);

    if (dest_width_inout)
        *dest_width_inout = dest_width;
    if (dest_height_inout)
        *dest_height_inout = dest_height;
}
