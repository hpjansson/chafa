/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2024-2025 Hans Petter Jansson
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

#ifndef __CHICLE_GRID_LAYOUT_H__
#define __CHICLE_GRID_LAYOUT_H__

#include <glib.h>
#include "chafa-term.h"
#include "chicle-path-queue.h"

G_BEGIN_DECLS

typedef struct ChicleGridLayout ChicleGridLayout;

ChicleGridLayout *chicle_grid_layout_new (void);
void chicle_grid_layout_destroy (ChicleGridLayout *grid);

void chicle_grid_layout_set_path_queue (ChicleGridLayout *grid, ChiclePathQueue *path_queue);
void chicle_grid_layout_set_canvas_config (ChicleGridLayout *grid, ChafaCanvasConfig *canvas_config);
void chicle_grid_layout_set_term_info (ChicleGridLayout *grid, ChafaTermInfo *term_info);
void chicle_grid_layout_set_view_size (ChicleGridLayout *grid, gint width, gint height);
void chicle_grid_layout_set_grid_size (ChicleGridLayout *grid, gint n_cols, gint n_rows);
void chicle_grid_layout_set_align (ChicleGridLayout *grid, ChafaAlign halign, ChafaAlign valign);
void chicle_grid_layout_set_tuck (ChicleGridLayout *grid, ChafaTuck tuck);
void chicle_grid_layout_set_print_labels (ChicleGridLayout *grid, gboolean print_labels);
void chicle_grid_layout_set_link_labels (ChicleGridLayout *grid, gboolean link_labels);
void chicle_grid_layout_set_use_unicode (ChicleGridLayout *grid, gboolean use_unicode);

gboolean chicle_grid_layout_print_chunk (ChicleGridLayout *grid, ChafaTerm *term);

G_END_DECLS

#endif /* __CHICLE_GRID_LAYOUT_H__ */
