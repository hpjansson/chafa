/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2024 Hans Petter Jansson
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

#ifndef __GRID_LAYOUT_H__
#define __GRID_LAYOUT_H__

#include <glib.h>
#include "chafa-term.h"

G_BEGIN_DECLS

typedef struct GridLayout GridLayout;

GridLayout *grid_layout_new (void);
void grid_layout_destroy (GridLayout *grid);

void grid_layout_set_canvas_config (GridLayout *grid, ChafaCanvasConfig *canvas_config);
void grid_layout_set_term_info (GridLayout *grid, ChafaTermInfo *term_info);
void grid_layout_set_view_size (GridLayout *grid, gint width, gint height);
void grid_layout_set_grid_size (GridLayout *grid, gint n_cols, gint n_rows);
void grid_layout_set_align (GridLayout *grid, ChafaAlign halign, ChafaAlign valign);
void grid_layout_set_tuck (GridLayout *grid, ChafaTuck tuck);

void grid_layout_push_path (GridLayout *grid, const gchar *path);

gboolean grid_layout_print_chunk (GridLayout *grid, ChafaTerm *term);

G_END_DECLS

#endif /* __GRID_LAYOUT_H__ */
