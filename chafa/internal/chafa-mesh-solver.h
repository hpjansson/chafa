/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2024 Hans Petter Jansson
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

#ifndef __CHAFA_MESH_SOLVER_H__
#define __CHAFA_MESH_SOLVER_H__

#include <glib.h>
#include "chafa.h"
#include "internal/chafa-private.h"
#include "internal/chafa-mesh.h"

G_BEGIN_DECLS

typedef gint (ChafaUpdateCellFunc) (ChafaWorkCell *work_cell, ChafaCanvasCell *cell_out,
                                    gpointer user_data);

typedef struct
{
    /* How poorly the chosen symbol matches the preferred shape */
    gint64 shape_error;

    /* How poorly the selected colors capture the color range present in the cell */
    gint64 color_error;

    /* How deformed the mesh is at this cell */
    gint64 mesh_error;

    /* Last optimization attempt of this cell @ iteration count */
    gint attempt_stamp;

    /* If the cell needs to be updated due to mesh changes */
    guint is_dirty : 1;
}
ChafaCellStats;

typedef struct
{
    guint16 x, y;
}
ChafaCellIndex;

typedef struct ChafaMeshSolver ChafaMeshSolver;

struct ChafaMeshSolver
{
    ChafaMesh *mesh;

    guint32 *pixels;
    gint pixels_width, pixels_height, pixels_rowstride;

    ChafaCanvasCell *cells;
    gint cells_width, cells_height, cells_rowstride;

    ChafaUpdateCellFunc *update_func;
    gpointer update_func_data;

    ChafaCellStats *stats;
    ChafaCellIndex *index_by_shape_error;
    ChafaCellIndex *index_by_color_error;
    ChafaCellIndex *index_by_mesh_error;
    ChafaCellIndex *index_by_priority;

    gint iterations;
};

void chafa_mesh_solver_init (ChafaMeshSolver *solver,
                             gpointer pixels,
                             gint pixels_width, gint pixels_height,
                             gint pixels_rowstride,
                             ChafaCanvasCell *cells,
                             gint cells_width, gint cells_height,
                             gint cells_rowstride,
                             ChafaUpdateCellFunc *update_func,
                             gpointer update_func_data);
void chafa_mesh_solver_deinit (ChafaMeshSolver *solver);
ChafaMeshSolver *chafa_mesh_solver_new (gpointer pixels,
                                        gint pixels_width, gint pixels_height,
                                        gint pixels_rowstride,
                                        ChafaCanvasCell *cells,
                                        gint cells_width, gint cells_height,
                                        gint cells_rowstride,
                                        ChafaUpdateCellFunc *update_func,
                                        gpointer update_func_data);

void chafa_mesh_solver_solve (ChafaMeshSolver *solver);

G_END_DECLS

#endif /* __CHAFA_MESH_SOLVER_H__ */
