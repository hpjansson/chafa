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

#ifndef __PLACEMENT_COUNTER_H__
#define __PLACEMENT_COUNTER_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct PlacementCounter PlacementCounter;

PlacementCounter *placement_counter_new (void);
void placement_counter_destroy (PlacementCounter *counter);

guint placement_counter_get_next_id (PlacementCounter *counter);

G_END_DECLS

#endif /* __PLACEMENT_COUNTER_H__ */
