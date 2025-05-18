/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2025 Hans Petter Jansson
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
 * SECTION:chafa-placement
 * @title: ChafaPlacement
 * @short_description: Placement of a #ChafaImage on a #ChafaCanvas
 *
 * A #ChafaPlacement describes how an image is placed on a #ChafaCanvas. It
 * contains information about the image, its alignment and tucking policy.
 **/

/**
 * chafa_placement_new:
 * @image: The image to place on the canvas
 * @id: An ID to assign to the placement, or <= 0 to assign one automatically
 *
 * Creates a new #ChafaPlacement.
 *
 * Returns: The new placement
 *
 * Since: 1.14
 **/
ChafaPlacement *
chafa_placement_new (ChafaImage *image, gint id)
{
    ChafaPlacement *placement;

    g_return_val_if_fail (image != NULL, NULL);

    if (id < 1)
        id = -1;

    placement = g_new0 (ChafaPlacement, 1);
    placement->refs = 1;

    chafa_image_ref (image);
    placement->image = image;
    placement->id = id;
    placement->halign = CHAFA_ALIGN_START;
    placement->valign = CHAFA_ALIGN_START;
    placement->tuck = CHAFA_TUCK_STRETCH;

    return placement;
}

/**
 * chafa_placement_ref:
 * @placement: Placement to add a reference to
 *
 * Adds a reference to @placement.
 *
 * Since: 1.14
 **/
void
chafa_placement_ref (ChafaPlacement *placement)
{
    gint refs;

    g_return_if_fail (placement != NULL);
    refs = g_atomic_int_get (&placement->refs);
    g_return_if_fail (refs > 0);

    g_atomic_int_inc (&placement->refs);
}

/**
 * chafa_placement_unref:
 * @placement: Placement to remove a reference from
 *
 * Removes a reference from @placement. When remaining references drops to
 * zero, the placement is freed and can no longer be used.
 *
 * Since: 1.14
 **/
void
chafa_placement_unref (ChafaPlacement *placement)
{
    gint refs;

    g_return_if_fail (placement != NULL);
    refs = g_atomic_int_get (&placement->refs);
    g_return_if_fail (refs > 0);

    if (g_atomic_int_dec_and_test (&placement->refs))
    {
        chafa_image_unref (placement->image);
        g_free (placement);
    }
}

/**
 * chafa_placement_get_tuck:
 * @placement: A #ChafaPlacement
 *
 * Gets the tucking policy of @placement. This describes how the image is
 * resized to fit @placement's extents, and defaults to #CHAFA_TUCK_STRETCH.
 *
 * Returns: The tucking policy of @placement
 *
 * Since: 1.14
 **/
ChafaTuck
chafa_placement_get_tuck (ChafaPlacement *placement)
{
    g_return_val_if_fail (placement != NULL, CHAFA_TUCK_FIT);

    return placement->tuck;
}

/**
 * chafa_placement_set_tuck:
 * @placement: A #ChafaPlacement
 * @tuck: Desired tucking policy
 *
 * Sets the tucking policy for @placement to @tuck. This describes how the image is
 * resized to fit @placement's extents, and defaults to #CHAFA_TUCK_STRETCH.
 *
 * Since: 1.14
 **/
void
chafa_placement_set_tuck (ChafaPlacement *placement, ChafaTuck tuck)
{
    g_return_if_fail (placement != NULL);

    placement->tuck = tuck;
}

/**
 * chafa_placement_get_halign:
 * @placement: A #ChafaPlacement
 *
 * Gets the horizontal alignment of @placement. This determines how any
 * padding added by the tucking policy is distributed, and defaults to
 * #CHAFA_ALIGN_START.
 *
 * Returns: The horizontal alignment of @placement
 *
 * Since: 1.14
 **/
ChafaAlign
chafa_placement_get_halign (ChafaPlacement *placement)
{
    g_return_val_if_fail (placement != NULL, CHAFA_ALIGN_START);

    return placement->halign;
}

/**
 * chafa_placement_set_halign:
 * @placement: A #ChafaPlacement
 * @align: Desired horizontal alignment
 *
 * Sets the horizontal alignment of @placement. This determines how any
 * padding added by the tucking policy is distributed, and defaults to
 * #CHAFA_ALIGN_START.
 *
 * Since: 1.14
 **/
void
chafa_placement_set_halign (ChafaPlacement *placement, ChafaAlign align)
{
    g_return_if_fail (placement != NULL);

    placement->halign = align;
}

/**
 * chafa_placement_get_valign:
 * @placement: A #ChafaPlacement
 *
 * Gets the vertical alignment of @placement. This determines how any
 * padding added by the tucking policy is distributed, and defaults to
 * #CHAFA_ALIGN_START.
 *
 * Returns: The vertical alignment of @placement
 *
 * Since: 1.14
 **/
ChafaAlign
chafa_placement_get_valign (ChafaPlacement *placement)
{
    g_return_val_if_fail (placement != NULL, CHAFA_ALIGN_START);

    return placement->valign;
}

/**
 * chafa_placement_set_valign:
 * @placement: A #ChafaPlacement
 * @align: Desired vertical alignment
 *
 * Sets the vertical alignment of @placement. This determines how any
 * padding added by the tucking policy is distributed.
 *
 * Since: 1.14
 **/
void
chafa_placement_set_valign (ChafaPlacement *placement, ChafaAlign align)
{
    g_return_if_fail (placement != NULL);

    placement->valign = align;
}
