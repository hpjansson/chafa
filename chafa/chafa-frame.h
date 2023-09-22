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

#ifndef __CHAFA_FRAME_H__
#define __CHAFA_FRAME_H__

#if !defined (__CHAFA_H_INSIDE__) && !defined (CHAFA_COMPILATION)
# error "Only <chafa.h> can be included directly."
#endif

G_BEGIN_DECLS

typedef struct ChafaFrame ChafaFrame;

CHAFA_AVAILABLE_IN_1_14
ChafaFrame *chafa_frame_new (gconstpointer data,
                             ChafaPixelType pixel_type,
                             gint width, gint height, gint rowstride);
CHAFA_AVAILABLE_IN_1_14
ChafaFrame *chafa_frame_new_steal (gpointer data,
                                   ChafaPixelType pixel_type,
                                   gint width, gint height, gint rowstride);
CHAFA_AVAILABLE_IN_1_14
ChafaFrame *chafa_frame_new_borrow (gpointer data,
                                    ChafaPixelType pixel_type,
                                    gint width, gint height, gint rowstride);

CHAFA_AVAILABLE_IN_1_14
void chafa_frame_ref (ChafaFrame *frame);
CHAFA_AVAILABLE_IN_1_14
void chafa_frame_unref (ChafaFrame *frame);

G_END_DECLS

#endif /* __CHAFA_FRAME_H__ */
