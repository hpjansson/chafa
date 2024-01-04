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

/**
 * ChafaAlign:
 * @CHAFA_ALIGN_START: Align flush with beginning of the area (top or left in LTR locales).
 * @CHAFA_ALIGN_END: Align flush with end of the area (bottom or right in LTR locales).
 * @CHAFA_ALIGN_CENTER: Align in the middle of the area.
 * @CHAFA_ALIGN_MAX: Last supported alignment, plus one.
 *
 * Alignment options when placing an element within an area.
 *
 * Since: 1.14
 **/

typedef enum
{
    CHAFA_ALIGN_START,
    CHAFA_ALIGN_END,
    CHAFA_ALIGN_CENTER,

    CHAFA_ALIGN_MAX
}
ChafaAlign;

/**
 * ChafaTuck:
 * @CHAFA_TUCK_STRETCH: Resize element to fit the area exactly, changing its aspect ratio.
 * @CHAFA_TUCK_FIT: Resize element to fit the area, preserving its aspect ratio by adding padding.
 * @CHAFA_TUCK_SHRINK_TO_FIT: Like @CHAFA_TUCK_FIT, but prohibit enlargement.
 * @CHAFA_TUCK_MAX: Last supported tucking policy, plus one.
 *
 * Resizing options when placing an element within an area. Usually used in conjunction
 * with #ChafaAlign to control the padding.
 *
 * Since: 1.14
 **/

typedef enum
{
    CHAFA_TUCK_STRETCH,
    CHAFA_TUCK_FIT,
    CHAFA_TUCK_SHRINK_TO_FIT,

    CHAFA_TUCK_MAX
}
ChafaTuck;

G_END_DECLS

#endif /* __CHAFA_COMMON_H__ */
