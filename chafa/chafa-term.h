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

#ifndef __CHAFA_TERM_H__
#define __CHAFA_TERM_H__

#if !defined (__CHAFA_H_INSIDE__) && !defined (CHAFA_COMPILATION)
# error "Only <chafa.h> can be included directly."
#endif

#include <glib.h>
#include "chafa-parser.h"

G_BEGIN_DECLS

typedef struct ChafaTerm ChafaTerm;

CHAFA_AVAILABLE_IN_1_20
ChafaTerm *chafa_term_new (ChafaTermInfo *term_info, gint in_fd, gint out_fd, gint err_fd);
CHAFA_AVAILABLE_IN_1_20
void chafa_term_destroy (ChafaTerm *term);

CHAFA_AVAILABLE_IN_1_20
ChafaTerm *chafa_term_get_default (void);

CHAFA_AVAILABLE_IN_1_20
gint chafa_term_get_buffer_max (ChafaTerm *term);
CHAFA_AVAILABLE_IN_1_20
void chafa_term_set_buffer_max (ChafaTerm *term, gint max);

CHAFA_AVAILABLE_IN_1_20
ChafaTermInfo *chafa_term_get_term_info (ChafaTerm *term);

CHAFA_AVAILABLE_IN_1_20
ChafaEvent *chafa_term_read_event (ChafaTerm *term, guint timeout_ms);

CHAFA_AVAILABLE_IN_1_20
void chafa_term_write (ChafaTerm *term, gconstpointer data, gint len);
CHAFA_AVAILABLE_IN_1_20
gint chafa_term_print (ChafaTerm *term, const gchar *format, ...) G_GNUC_PRINTF (2, 3);
CHAFA_AVAILABLE_IN_1_20
gint chafa_term_print_seq (ChafaTerm *term, ChafaTermSeq seq, ...);
CHAFA_AVAILABLE_IN_1_20
gboolean chafa_term_flush (ChafaTerm *term);

CHAFA_AVAILABLE_IN_1_20
void chafa_term_write_err (ChafaTerm *term, gconstpointer data, gint len);
CHAFA_AVAILABLE_IN_1_20
gint chafa_term_print_err (ChafaTerm *term, const gchar *format, ...) G_GNUC_PRINTF (2, 3);

CHAFA_AVAILABLE_IN_1_20
void chafa_term_get_size_px (ChafaTerm *term, gint *width_px_out, gint *height_px_out);
CHAFA_AVAILABLE_IN_1_20
void chafa_term_get_size_cells (ChafaTerm *term, gint *width_cells_out, gint *height_cells_out);

CHAFA_AVAILABLE_IN_1_20
gboolean chafa_term_sync_probe (ChafaTerm *term, gint timeout_ms);

CHAFA_AVAILABLE_IN_1_20
void chafa_term_notify_size_changed (ChafaTerm *term);

CHAFA_AVAILABLE_IN_1_20
gint32 chafa_term_get_default_fg_color (ChafaTerm *term);
CHAFA_AVAILABLE_IN_1_20
gint32 chafa_term_get_default_bg_color (ChafaTerm *term);

G_END_DECLS

#endif /* __CHAFA_TERM_H__ */
