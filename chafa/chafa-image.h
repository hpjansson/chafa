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

#ifndef __CHAFA_IMAGE_H__
#define __CHAFA_IMAGE_H__

#if !defined (__CHAFA_H_INSIDE__) && !defined (CHAFA_COMPILATION)
# error "Only <chafa.h> can be included directly."
#endif

G_BEGIN_DECLS

typedef struct ChafaImage ChafaImage;

CHAFA_AVAILABLE_IN_1_14
ChafaImage *chafa_image_new (void);
CHAFA_AVAILABLE_IN_1_14
void chafa_image_ref (ChafaImage *image);
CHAFA_AVAILABLE_IN_1_14
void chafa_image_unref (ChafaImage *image);

CHAFA_AVAILABLE_IN_1_14
void chafa_image_set_frame (ChafaImage *image, ChafaFrame *frame);

G_END_DECLS

#endif /* __CHAFA_IMAGE_H__ */
