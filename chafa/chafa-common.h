/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2024 Hans Petter Jansson
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

/* Color extractor */

/**
 * ChafaColorExtractor:
 * @CHAFA_COLOR_EXTRACTOR_AVERAGE: Use the average colors of each symbol's coverage area.
 * @CHAFA_COLOR_EXTRACTOR_MEDIAN: Use the median colors of each symbol's coverage area.
 * @CHAFA_COLOR_EXTRACTOR_MAX: Last supported color extractor plus one.
 **/

typedef enum
{
    CHAFA_COLOR_EXTRACTOR_AVERAGE,
    CHAFA_COLOR_EXTRACTOR_MEDIAN,

    CHAFA_COLOR_EXTRACTOR_MAX
}
ChafaColorExtractor;

/* Color spaces */

/**
 * ChafaColorSpace:
 * @CHAFA_COLOR_SPACE_RGB: RGB color space. Fast but imprecise.
 * @CHAFA_COLOR_SPACE_DIN99D: DIN99d color space. Slower, but good perceptual color precision.
 * @CHAFA_COLOR_SPACE_MAX: Last supported color space plus one.
 **/

typedef enum
{
    CHAFA_COLOR_SPACE_RGB,
    CHAFA_COLOR_SPACE_DIN99D,

    CHAFA_COLOR_SPACE_MAX
}
ChafaColorSpace;

/* Dither modes */

/**
 * ChafaDitherMode:
 * @CHAFA_DITHER_MODE_NONE: No dithering.
 * @CHAFA_DITHER_MODE_ORDERED: Ordered dithering (Bayer or similar).
 * @CHAFA_DITHER_MODE_DIFFUSION: Error diffusion dithering (Floyd-Steinberg or similar).
 * @CHAFA_DITHER_MODE_NOISE: Noise pattern dithering (blue noise or similar).
 * @CHAFA_DITHER_MODE_MAX: Last supported dither mode plus one.
 **/

typedef enum
{
    CHAFA_DITHER_MODE_NONE,
    CHAFA_DITHER_MODE_ORDERED,
    CHAFA_DITHER_MODE_DIFFUSION,
    CHAFA_DITHER_MODE_NOISE,

    CHAFA_DITHER_MODE_MAX
}
ChafaDitherMode;

/* Sequence optimization flags. When enabled, these may produce more compact
 * output at the cost of reduced compatibility and increased CPU use. Output
 * quality is unaffected. */

/**
 * ChafaOptimizations:
 * @CHAFA_OPTIMIZATION_REUSE_ATTRIBUTES: Suppress redundant SGR control sequences.
 * @CHAFA_OPTIMIZATION_SKIP_CELLS: Reserved for future use.
 * @CHAFA_OPTIMIZATION_REPEAT_CELLS: Use REP sequence to compress repeated runs of similar cells.
 * @CHAFA_OPTIMIZATION_NONE: All optimizations disabled.
 * @CHAFA_OPTIMIZATION_ALL: All optimizations enabled.
 **/

typedef enum
{
    CHAFA_OPTIMIZATION_REUSE_ATTRIBUTES = (1 << 0),
    CHAFA_OPTIMIZATION_SKIP_CELLS = (1 << 1),
    CHAFA_OPTIMIZATION_REPEAT_CELLS = (1 << 2),

    CHAFA_OPTIMIZATION_NONE = 0,
    CHAFA_OPTIMIZATION_ALL = 0x7fffffff
}
ChafaOptimizations;

/* Canvas modes */

/**
 * ChafaCanvasMode:
 * @CHAFA_CANVAS_MODE_TRUECOLOR: Truecolor.
 * @CHAFA_CANVAS_MODE_INDEXED_256: 256 colors.
 * @CHAFA_CANVAS_MODE_INDEXED_240: 256 colors, but avoid using the lower 16 whose values vary between terminal environments.
 * @CHAFA_CANVAS_MODE_INDEXED_16: 16 colors using the aixterm ANSI extension.
 * @CHAFA_CANVAS_MODE_INDEXED_16_8: 16 FG colors (8 of which enabled with bold/bright) and 8 BG colors.
 * @CHAFA_CANVAS_MODE_INDEXED_8: 8 colors, compatible with original ANSI X3.64.
 * @CHAFA_CANVAS_MODE_FGBG_BGFG: Default foreground and background colors, plus inversion.
 * @CHAFA_CANVAS_MODE_FGBG: Default foreground and background colors. No ANSI codes will be used.
 * @CHAFA_CANVAS_MODE_MAX: Last supported canvas mode plus one.
 **/

typedef enum
{
    CHAFA_CANVAS_MODE_TRUECOLOR,
    CHAFA_CANVAS_MODE_INDEXED_256,
    CHAFA_CANVAS_MODE_INDEXED_240,
    CHAFA_CANVAS_MODE_INDEXED_16,
    CHAFA_CANVAS_MODE_FGBG_BGFG,
    CHAFA_CANVAS_MODE_FGBG,
    CHAFA_CANVAS_MODE_INDEXED_8,
    CHAFA_CANVAS_MODE_INDEXED_16_8,

    CHAFA_CANVAS_MODE_MAX
}
ChafaCanvasMode;

/* Pixel modes */

/**
 * ChafaPixelMode:
 * @CHAFA_PIXEL_MODE_SYMBOLS: Pixel data is approximated using character symbols ("ANSI art").
 * @CHAFA_PIXEL_MODE_SIXELS: Pixel data is encoded as sixels.
 * @CHAFA_PIXEL_MODE_KITTY: Pixel data is encoded using the Kitty terminal protocol.
 * @CHAFA_PIXEL_MODE_ITERM2: Pixel data is encoded using the iTerm2 terminal protocol.
 * @CHAFA_PIXEL_MODE_MAX: Last supported pixel mode plus one.
 **/

typedef enum
{
    CHAFA_PIXEL_MODE_SYMBOLS,
    CHAFA_PIXEL_MODE_SIXELS,
    CHAFA_PIXEL_MODE_KITTY,
    CHAFA_PIXEL_MODE_ITERM2,

    CHAFA_PIXEL_MODE_MAX
}
ChafaPixelMode;

/* Passthrough modes */

/**
 * ChafaPassthrough:
 * @CHAFA_PASSTHROUGH_NONE: No passthrough guards will be used.
 * @CHAFA_PASSTHROUGH_SCREEN: Passthrough guards for GNU Screen will be used.
 * @CHAFA_PASSTHROUGH_TMUX: Passthrough guards for tmux will be used.
 * @CHAFA_PASSTHROUGH_MAX: Last supported passthrough mode plus one.
 **/

typedef enum
{
    CHAFA_PASSTHROUGH_NONE,
    CHAFA_PASSTHROUGH_SCREEN,
    CHAFA_PASSTHROUGH_TMUX,

    CHAFA_PASSTHROUGH_MAX
}
ChafaPassthrough;

G_END_DECLS

#endif /* __CHAFA_COMMON_H__ */
