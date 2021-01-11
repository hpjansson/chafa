/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2020-2021 Hans Petter Jansson
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

#ifndef __CHAFA_CANVAS_PRINTER_H__
#define __CHAFA_CANVAS_PRINTER_H__

#include <glib.h>
#include "chafa.h"
#include "internal/chafa-canvas-internal.h"
#include "internal/chafa-private.h"
#include "internal/chafa-pixops.h"

G_BEGIN_DECLS

GString *chafa_canvas_print_symbols (ChafaCanvas *canvas, ChafaTermInfo *ti);

G_END_DECLS

#endif /* __CHAFA_CANVAS_PRINTER_H__ */
