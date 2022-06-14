/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2022 Hans Petter Jansson
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

#include "config.h"

#include <stdlib.h>  /* abs */
#include <math.h>  /* pow, cbrt, log, sqrt, atan2, cos, sin */
#include "chafa.h"
#include "internal/chafa-color.h"

#define DEBUG(x)

guint32
chafa_pack_color (const ChafaColor *color)
{
    /* Assumes each channel 0 <= value <= 255 */
    return ((guint32) color->ch [0] << 16)
            | ((guint32) color->ch [1] << 8)
            | ((guint32) color->ch [2])
            | ((guint32) color->ch [3] << 24);  /* Alpha */
}

void
chafa_unpack_color (guint32 packed, ChafaColor *color_out)
{
    color_out->ch [0] = (packed >> 16) & 0xff;
    color_out->ch [1] = (packed >> 8) & 0xff;
    color_out->ch [2] = packed & 0xff;
    color_out->ch [3] = (packed >> 24) & 0xff;  /* Alpha */
}

void
chafa_color_accum_div_scalar (ChafaColorAccum *accum, gint scalar)
{
    accum->ch [0] /= scalar;
    accum->ch [1] /= scalar;
    accum->ch [2] /= scalar;
    accum->ch [3] /= scalar;
}

typedef struct
{
    gdouble c [3];
}
ChafaColorRGBf;

typedef struct
{
    gdouble c [3];
}
ChafaColorXYZ;

typedef struct
{
    gdouble c [3];
}
ChafaColorLab;

static gdouble
invert_rgb_channel_compand (gdouble v)
{
    return v <= 0.04045 ? (v / 12.92) : pow ((v + 0.055) / 1.044, 2.4);
}

static void
convert_rgb_to_xyz (const ChafaColor *rgbi, ChafaColorXYZ *xyz)
{
    ChafaColorRGBf rgbf;
    gint i;

    for (i = 0; i < 3; i++)
    {
        rgbf.c [i] = (gdouble) rgbi->ch [i] / 255.0;
        rgbf.c [i] = invert_rgb_channel_compand (rgbf.c [i]);
    }

    xyz->c [0] = 0.4124564 * rgbf.c [0] + 0.3575761 * rgbf.c [1] + 0.1804375 * rgbf.c [2];
    xyz->c [1] = 0.2126729 * rgbf.c [0] + 0.7151522 * rgbf.c [1] + 0.0721750 * rgbf.c [2];
    xyz->c [2] = 0.0193339 * rgbf.c [0] + 0.1191920 * rgbf.c [1] + 0.9503041 * rgbf.c [2];
}

#define XYZ_EPSILON (216.0 / 24389.0)
#define XYZ_KAPPA (24389.0 / 27.0)

static gdouble
lab_f (gdouble v)
{
    return v > XYZ_EPSILON ? cbrt (v) : (XYZ_KAPPA * v + 16.0) / 116.0;
}

static void
convert_xyz_to_lab (const ChafaColorXYZ *xyz, ChafaColorLab *lab)
{
    ChafaColorXYZ wp = { { 0.95047, 1.0, 1.08883 } };  /* D65 white point */
    ChafaColorXYZ xyz2;
    gint i;

    for (i = 0; i < 3; i++)
        xyz2.c [i] = lab_f (xyz->c [i] / wp.c [i]);

    lab->c [0] = 116.0 * xyz2.c [1] - 16.0;
    lab->c [1] = 500.0 * (xyz2.c [0] - xyz2.c [1]);
    lab->c [2] = 200.0 * (xyz2.c [1] - xyz2.c [2]);
}

void
chafa_color_rgb_to_din99d (const ChafaColor *rgb, ChafaColor *din99)
{
    ChafaColorXYZ xyz;
    ChafaColorLab lab;
    gdouble adj_L, ee, f, G, C, h;

    convert_rgb_to_xyz (rgb, &xyz);

    /* Apply tristimulus-space correction term */

    xyz.c [0] = 1.12 * xyz.c [0] - 0.12 * xyz.c [2];

    /* Convert to L*a*b* */

    convert_xyz_to_lab (&xyz, &lab);
    adj_L = 325.22 * log (1.0 + 0.0036 * lab.c [0]);

    /* Intermediate parameters */

    ee = 0.6427876096865393 * lab.c [1] + 0.766044443118978 * lab.c [2];
    f = 1.14 * (0.6427876096865393 * lab.c [2] - 0.766044443118978 * lab.c [1]);
    G = sqrt (ee * ee + f * f);

    /* Hue/chroma */

    C = 22.5 * log (1.0 + 0.06 * G);

    h = atan2 (f, ee) + 0.8726646 /* 50 degrees */;
    while (h < 0.0) h += 6.283185;  /* 360 degrees */
    while (h > 6.283185) h -= 6.283185;  /* 360 degrees */

    /* The final values should be in the range [0..255] */

    din99->ch [0] = adj_L * 2.5;
    din99->ch [1] = C * cos (h) * 2.5 + 128.0;
    din99->ch [2] = C * sin (h) * 2.5 + 128.0;
    din99->ch [3] = rgb->ch [3];
}
