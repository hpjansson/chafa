/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2023 Hans Petter Jansson
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

#ifndef __CHAFA_PLACEMENT_H__
#define __CHAFA_PLACEMENT_H__

#if !defined (__CHAFA_H_INSIDE__) && !defined (CHAFA_COMPILATION)
# error "Only <chafa.h> can be included directly."
#endif

G_BEGIN_DECLS

typedef struct ChafaPlacement ChafaPlacement;

CHAFA_AVAILABLE_IN_1_14
ChafaPlacement *chafa_placement_new (ChafaImage *image, gint id);
CHAFA_AVAILABLE_IN_1_14
void chafa_placement_ref (ChafaPlacement *placement);
CHAFA_AVAILABLE_IN_1_14
void chafa_placement_unref (ChafaPlacement *placement);

CHAFA_AVAILABLE_IN_1_14
ChafaTuck chafa_placement_get_tuck (ChafaPlacement *placement);
CHAFA_AVAILABLE_IN_1_14
void chafa_placement_set_tuck (ChafaPlacement *placement, ChafaTuck tuck);

CHAFA_AVAILABLE_IN_1_14
ChafaAlign chafa_placement_get_halign (ChafaPlacement *placement);
CHAFA_AVAILABLE_IN_1_14
void chafa_placement_set_halign (ChafaPlacement *placement, ChafaAlign align);

CHAFA_AVAILABLE_IN_1_14
ChafaAlign chafa_placement_get_valign (ChafaPlacement *placement);
CHAFA_AVAILABLE_IN_1_14
void chafa_placement_set_valign (ChafaPlacement *placement, ChafaAlign align);

G_END_DECLS

#endif /* __CHAFA_PLACEMENT_H__ */
