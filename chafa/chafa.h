/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018 Hans Petter Jansson
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

#ifndef __CHAFA_H__
#define __CHAFA_H__
#define __CHAFA_H_INSIDE__

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
    /* 32 bits per pixel */

    CHAFA_PIXEL_RGBA8_PREMULTIPLIED,
    CHAFA_PIXEL_BGRA8_PREMULTIPLIED,
    CHAFA_PIXEL_ARGB8_PREMULTIPLIED,
    CHAFA_PIXEL_ABGR8_PREMULTIPLIED,

    CHAFA_PIXEL_RGBA8_UNASSOCIATED,
    CHAFA_PIXEL_BGRA8_UNASSOCIATED,
    CHAFA_PIXEL_ARGB8_UNASSOCIATED,
    CHAFA_PIXEL_ABGR8_UNASSOCIATED,

    /* 24 bits per pixel */

    CHAFA_PIXEL_RGB8,
    CHAFA_PIXEL_BGR8,

    CHAFA_PIXEL_MAX
}
ChafaPixelType;

/* Version macros go before everything else */
#include <chafa-version-macros.h>

#include <chafa-features.h>
#include <chafa-canvas-config.h>
#include <chafa-canvas.h>
#include <chafa-symbol-map.h>
#include <chafa-util.h>

G_END_DECLS

#undef __CHAFA_H_INSIDE__
#endif /* __CHAFA_H__ */
