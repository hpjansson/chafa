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

#ifndef __CHAFA_MESH_H__
#define __CHAFA_MESH_H__

#include <glib.h>
#include "chafa.h"
#include "internal/chafa-work-cell.h"

G_BEGIN_DECLS

typedef struct ChafaMesh ChafaMesh;

struct ChafaMesh
{
    ChafaVec2f32 *grid;
    ChafaVec2f32 *saved_grid;
    gint grid_width, grid_height;
    gint width_pixels, height_pixels;

    ChafaVec2f32 cell_scale;
    ChafaVec2f32 cell_scale_inv;
};

void chafa_mesh_init (ChafaMesh *mesh,
                      gint width_pixels, gint height_pixels,
                      gint width_cells, gint height_cells);

void chafa_mesh_deinit (ChafaMesh *mesh);

ChafaMesh *chafa_mesh_new (gint width_pixels, gint height_pixels,
                           gint width_cells, gint height_cells);

void chafa_mesh_save_point (ChafaMesh *mesh, gint x, gint y);
void chafa_mesh_save_all (ChafaMesh *mesh);

void chafa_mesh_restore_point (ChafaMesh *mesh, gint x, gint y);
void chafa_mesh_restore_all (ChafaMesh *mesh);

gboolean chafa_mesh_nudge_point (ChafaMesh *mesh, gint x, gint y,
                                 gfloat x_step, gfloat y_step);
gboolean chafa_mesh_perturb_point (ChafaMesh *mesh, gint x, gint y, gfloat step_factor);

void chafa_mesh_translate_rect (ChafaMesh *mesh, gint x, gint y,
                                gint width, gint height,
                                gfloat x_step, gfloat y_step);
void chafa_mesh_scale_rect (ChafaMesh *mesh, gint x, gint y,
                            gint width, gint height,
                            gfloat x_factor, gfloat y_factor);

gfloat chafa_mesh_get_cell_drift (ChafaMesh *mesh, gint x, gint y);
gfloat chafa_mesh_get_cell_deform (ChafaMesh *mesh, gint x, gint y);
gfloat chafa_mesh_get_cell_misalign (ChafaMesh *mesh, gint x, gint y);

gfloat chafa_mesh_get_absolute_deformity (ChafaMesh *mesh, gint x, gint y);
gfloat chafa_mesh_get_relative_deformity (ChafaMesh *mesh, gint x, gint y);

void chafa_mesh_sample_cell (ChafaMesh *mesh, ChafaWorkCell *work_cell_out,
                             gint x, gint y,
                             gconstpointer pixels,
                             gint pixels_width, gint pixels_height,
                             gint pixels_rowstride);

void
chafa_mesh_oversample_cell (ChafaMesh *mesh, ChafaWorkCell *work_cell_out,
                            gint x, gint y,
                            gconstpointer pixels,
                            gint pixels_width, gint pixels_height,
                            gint pixels_rowstride,
                            gfloat oversample_amt);

G_END_DECLS

#endif /* __CHAFA_MESH_H__ */
