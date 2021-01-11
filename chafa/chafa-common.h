/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2021 Hans Petter Jansson
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

#ifndef __CHAFA_COMMON_H__
#define __CHAFA_COMMON_H__

#if !defined (__CHAFA_H_INSIDE__) && !defined (CHAFA_COMPILATION)
# error "Only <chafa.h> can be included directly."
#endif

G_BEGIN_DECLS

/**
 * ChafaPixelType:
 * @CHAFA_PIXEL_RGBA8_PREMULTIPLIED: Premultiplied RGBA, 8 bits per channel.
 * @CHAFA_PIXEL_BGRA8_PREMULTIPLIED: Premultiplied BGRA, 8 bits per channel.
 * @CHAFA_PIXEL_ARGB8_PREMULTIPLIED: Premultiplied ARGB, 8 bits per channel.
 * @CHAFA_PIXEL_ABGR8_PREMULTIPLIED: Premultiplied ABGR, 8 bits per channel.
 * @CHAFA_PIXEL_RGBA8_UNASSOCIATED: Unassociated RGBA, 8 bits per channel.
 * @CHAFA_PIXEL_BGRA8_UNASSOCIATED: Unassociated BGRA, 8 bits per channel.
 * @CHAFA_PIXEL_ARGB8_UNASSOCIATED: Unassociated ARGB, 8 bits per channel.
 * @CHAFA_PIXEL_ABGR8_UNASSOCIATED: Unassociated ABGR, 8 bits per channel.
 * @CHAFA_PIXEL_RGB8: Packed RGB (no alpha), 8 bits per channel.
 * @CHAFA_PIXEL_BGR8: Packed BGR (no alpha), 8 bits per channel.
 * @CHAFA_PIXEL_MAX: Last supported pixel type, plus one.
 *
 * Pixel formats supported by #ChafaCanvas and #ChafaSymbolMap.
 *
 * Since: 1.4
 **/

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

G_END_DECLS

#endif /* __CHAFA_COMMON_H__ */
