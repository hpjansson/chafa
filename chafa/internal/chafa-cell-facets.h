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

#ifndef __CHAFA_CELL_FACETS_H__
#define __CHAFA_CELL_FACETS_H__

#include "chafa.h"

G_BEGIN_DECLS

#define CHAFA_CELL_FACETS_N_SAMPLES ((CHAFA_SYMBOL_WIDTH_PIXELS + 1) * (CHAFA_SYMBOL_HEIGHT_PIXELS + 1))
#define CHAFA_CELL_FACETS_N_SECTORS_PER_QUADRANT 10
#define CHAFA_CELL_FACETS_N_SECTORS (CHAFA_CELL_FACETS_N_SECTORS_PER_QUADRANT * 4)
#define CHAFA_CELL_FACETS_N_FACETS (CHAFA_CELL_FACETS_N_SAMPLES * CHAFA_CELL_FACETS_N_SECTORS)

typedef struct ChafaCellFacets ChafaCellFacets;

struct ChafaCellFacets
{
    guint16 facets [CHAFA_CELL_FACETS_N_FACETS];
};

void chafa_cell_facets_from_bitmap (ChafaCellFacets *cell_facets_out, guint64 bitmap);
gint64 chafa_cell_facets_distance (const ChafaCellFacets *a, const ChafaCellFacets *b);

G_END_DECLS

#endif /* __CHAFA_CELL_FACETS_H__ */
