#include <stdlib.h>
#include <math.h>
#include <glib.h>
#include "chafa/chafa.h"

#define N_TERM_COLORS 257

/* 256-color values */
static guint32 term_colors_256 [N_TERM_COLORS] =
{
    0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080, 0x008080, 0xc0c0c0,
    /* 0x808080 -> */ 0x555555, 0xff0000, 0x00ff00, 0xffff00, 0x0000ff, 0xff00ff, 0x00ffff, 0xffffff,

    0x000000, 0x00005f, 0x000087, 0x0000af, 0x0000d7, 0x0000ff, 0x005f00, 0x005f5f,
    0x005f87, 0x005faf, 0x005fd7, 0x005fff, 0x008700, 0x00875f, 0x008787, 0x0087af,

    0x0087d7, 0x0087ff, 0x00af00, 0x00af5f, 0x00af87, 0x00afaf, 0x00afd7, 0x00afff,
    0x00d700, 0x00d75f, 0x00d787, 0x00d7af, 0x00d7d7, 0x00d7ff, 0x00ff00, 0x00ff5f,

    0x00ff87, 0x00ffaf, 0x00ffd7, 0x00ffff, 0x5f0000, 0x5f005f, 0x5f0087, 0x5f00af,
    0x5f00d7, 0x5f00ff, 0x5f5f00, 0x5f5f5f, 0x5f5f87, 0x5f5faf, 0x5f5fd7, 0x5f5fff,

    0x5f8700, 0x5f875f, 0x5f8787, 0x5f87af, 0x5f87d7, 0x5f87ff, 0x5faf00, 0x5faf5f,
    0x5faf87, 0x5fafaf, 0x5fafd7, 0x5fafff, 0x5fd700, 0x5fd75f, 0x5fd787, 0x5fd7af,

    0x5fd7d7, 0x5fd7ff, 0x5fff00, 0x5fff5f, 0x5fff87, 0x5fffaf, 0x5fffd7, 0x5fffff,
    0x870000, 0x87005f, 0x870087, 0x8700af, 0x8700d7, 0x8700ff, 0x875f00, 0x875f5f,

    0x875f87, 0x875faf, 0x875fd7, 0x875fff, 0x878700, 0x87875f, 0x878787, 0x8787af,
    0x8787d7, 0x8787ff, 0x87af00, 0x87af5f, 0x87af87, 0x87afaf, 0x87afd7, 0x87afff,

    0x87d700, 0x87d75f, 0x87d787, 0x87d7af, 0x87d7d7, 0x87d7ff, 0x87ff00, 0x87ff5f,
    0x87ff87, 0x87ffaf, 0x87ffd7, 0x87ffff, 0xaf0000, 0xaf005f, 0xaf0087, 0xaf00af,

    0xaf00d7, 0xaf00ff, 0xaf5f00, 0xaf5f5f, 0xaf5f87, 0xaf5faf, 0xaf5fd7, 0xaf5fff,
    0xaf8700, 0xaf875f, 0xaf8787, 0xaf87af, 0xaf87d7, 0xaf87ff, 0xafaf00, 0xafaf5f,

    0xafaf87, 0xafafaf, 0xafafd7, 0xafafff, 0xafd700, 0xafd75f, 0xafd787, 0xafd7af,
    0xafd7d7, 0xafd7ff, 0xafff00, 0xafff5f, 0xafff87, 0xafffaf, 0xafffd7, 0xafffff,

    0xd70000, 0xd7005f, 0xd70087, 0xd700af, 0xd700d7, 0xd700ff, 0xd75f00, 0xd75f5f,
    0xd75f87, 0xd75faf, 0xd75fd7, 0xd75fff, 0xd78700, 0xd7875f, 0xd78787, 0xd787af,

    0xd787d7, 0xd787ff, 0xdfaf00, 0xdfaf5f, 0xdfaf87, 0xdfafaf, 0xdfafdf, 0xdfafff,
    0xdfdf00, 0xdfdf5f, 0xdfdf87, 0xdfdfaf, 0xdfdfdf, 0xdfdfff, 0xdfff00, 0xdfff5f,

    0xdfff87, 0xdfffaf, 0xdfffdf, 0xdfffff, 0xff0000, 0xff005f, 0xff0087, 0xff00af,
    0xff00df, 0xff00ff, 0xff5f00, 0xff5f5f, 0xff5f87, 0xff5faf, 0xff5fdf, 0xff5fff,

    0xff8700, 0xff875f, 0xff8787, 0xff87af, 0xff87df, 0xff87ff, 0xffaf00, 0xffaf5f,
    0xffaf87, 0xffafaf, 0xffafdf, 0xffafff, 0xffdf00, 0xffdf5f, 0xffdf87, 0xffdfaf,

    0xffdfdf, 0xffdfff, 0xffff00, 0xffff5f, 0xffff87, 0xffffaf, 0xffffdf, 0xffffff,
    0x080808, 0x121212, 0x1c1c1c, 0x262626, 0x303030, 0x3a3a3a, 0x444444, 0x4e4e4e,

    0x585858, 0x626262, 0x6c6c6c, 0x767676, 0x808080, 0x8a8a8a, 0x949494, 0x9e9e9e,
    0xa8a8a8, 0xb2b2b2, 0xbcbcbc, 0xc6c6c6, 0xd0d0d0, 0xdadada, 0xe4e4e4, 0xeeeeee,

    0x808080  /* Transparent */
};

static ChafaPaletteColor palette_256 [N_TERM_COLORS];
static gboolean palette_initialized;

void
chafa_init_palette (void)
{
  gint i;

  if (palette_initialized)
    return;

  for (i = 0; i < N_TERM_COLORS; i++)
  {
    chafa_unpack_color (term_colors_256 [i], &palette_256 [i].col [0]);
    chafa_color_rgb_to_din99d (&palette_256 [i].col [0], &palette_256 [i].col [1]);

    palette_256 [i].col [0].ch [3] = 0xff;  /* Fully opaque */
    palette_256 [i].col [1].ch [3] = 0xff;  /* Fully opaque */
  }

  /* Transparent color */
  palette_256 [CHAFA_PALETTE_INDEX_TRANSPARENT].col [0].ch [3] = 0x00;
  palette_256 [CHAFA_PALETTE_INDEX_TRANSPARENT].col [1].ch [3] = 0x00;

  palette_initialized = TRUE;
}

const ChafaColor *
chafa_get_palette_color_256 (guint index, ChafaColorSpace color_space)
{
    return &palette_256 [index].col [color_space];
}

guint32
chafa_pack_color (const ChafaColor *color)
{
    /* Assumes each channel 0 <= value <= 255 */
    return (color->ch [0] << 16)
            | (color->ch [1] << 8)
            | (color->ch [2])
            | (color->ch [3] << 24);  /* Alpha */
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
chafa_color_div_scalar (ChafaColor *color, gint scalar)
{
    color->ch [0] /= scalar;
    color->ch [1] /= scalar;
    color->ch [2] /= scalar;
    color->ch [3] /= scalar;
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

static gdouble
rad_to_deg (gdouble r)
{
    gdouble deg = r * (180.0 / M_PI);
    while (deg < 0.0) deg += 360.0;
    while (deg > 360.0) deg -= 360.0;
    return deg;
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

    din99->ch [0] = adj_L * 16.0;
    din99->ch [1] = C * cos (h) * 16.0;
    din99->ch [2] = C * sin (h) * 16.0;
    din99->ch [3] = rgb->ch [3];
}

static gint
color_diff_rgb (const ChafaColor *col_a, const ChafaColor *col_b)
{
    gint error = 0;
    gint d [3];

    d [0] = col_b->ch [0] - col_a->ch [0];
    d [0] = d [0] * d [0];
    d [1] = col_b->ch [1] - col_a->ch [1];
    d [1] = d [1] * d [1];
    d [2] = col_b->ch [2] - col_a->ch [2];
    d [2] = d [2] * d [2];

    error = 2 * d [0] + 4 * d [1] + 3 * d [2]
        + (((col_a->ch [0] + col_b->ch [0]) / 2)
        * abs (d [0] - d [2])) / 256;

    return error;
}

static gint
color_diff_euclidean (const ChafaColor *col_a, const ChafaColor *col_b)
{
    gint error = 0;
    gint d [3];

    d [0] = col_b->ch [0] - col_a->ch [0];
    d [0] = d [0] * d [0];
    d [1] = col_b->ch [1] - col_a->ch [1];
    d [1] = d [1] * d [1];
    d [2] = col_b->ch [2] - col_a->ch [2];
    d [2] = d [2] * d [2];

    error = d [0] + d [1] + d [2];
    return error;
}

static gint
color_diff_alpha (const ChafaColor *col_a, const ChafaColor *col_b, gint error)
{
    gint max_opacity;
    gint a;

    /* Alpha */
    a = col_b->ch [3] - col_a->ch [3];
    a = a * a;
    max_opacity = MAX (col_a->ch [3], col_b->ch [3]);
    error *= max_opacity;
    error /= 256;
    error += a * 8;

    return error;
}

gint
chafa_color_diff_slow (const ChafaColor *col_a, const ChafaColor *col_b, ChafaColorSpace color_space)
{
    gint error;

    if (color_space == CHAFA_COLOR_SPACE_RGB)
        error = color_diff_rgb (col_a, col_b);
    else if (color_space == CHAFA_COLOR_SPACE_DIN99D)
        error = color_diff_euclidean (col_a, col_b);
    else
    {
        g_assert_not_reached ();
        return -1;
    }

    error = color_diff_alpha (col_a, col_b, error);

    return error;
}

/* FIXME: We may be able to avoid mixing alpha in most cases, but 16-color fill relies
 * on it at the moment. */
void
chafa_color_mix (ChafaColor *out, const ChafaColor *a, const ChafaColor *b, gint ratio)
{
    gint i;

    for (i = 0; i < 4; i++)
        out->ch [i] = (a->ch [i] * ratio + b->ch [i] * (1000 - ratio)) / 1000;
}

gint
chafa_pick_color_256 (const ChafaColor *color, ChafaColorSpace color_space)
{
    gint best_error = G_MAXINT;
    gint best_index = 0;
    gint i;

    for (i = 0; i < N_TERM_COLORS; i++)
    {
        const ChafaColor *palette_color;
        gint error;

        palette_color = chafa_get_palette_color_256 (i, color_space);
        error = chafa_color_diff_slow (color, palette_color, color_space);

        if (error < best_error)
        {
            best_index = i;
            best_error = error;
        }
    }

    return best_index;
}

gint
chafa_pick_color_240 (const ChafaColor *color, ChafaColorSpace color_space)
{
    gint best_error = G_MAXINT;
    gint best_index = 0;
    gint i;

    for (i = 16; i < N_TERM_COLORS; i++)
    {
        const ChafaColor *palette_color;
        gint error;

        palette_color = chafa_get_palette_color_256 (i, color_space);
        error = chafa_color_diff_slow (color, palette_color, color_space);

        if (error < best_error)
        {
            best_index = i;
            best_error = error;
        }
    }

    return best_index;
}

gint
chafa_pick_color_16 (const ChafaColor *color, ChafaColorSpace color_space)
{
    gint best_error = G_MAXINT;
    gint best_index = 0;
    const ChafaColor *palette_color;
    gint error;
    gint i;

    for (i = 0; i < 16; i++)
    {
        palette_color = chafa_get_palette_color_256 (i, color_space);
        error = chafa_color_diff_slow (color, palette_color, color_space);

        if (error < best_error)
        {
            best_index = i;
            best_error = error;
        }
    }

    /* Try transparency */

    palette_color = chafa_get_palette_color_256 (CHAFA_PALETTE_INDEX_TRANSPARENT, color_space);
    error = chafa_color_diff_slow (color, palette_color, color_space);

    if (error < best_error)
    {
        best_index = CHAFA_PALETTE_INDEX_TRANSPARENT;
        best_error = error;
    }

    return best_index;
}

gint
chafa_pick_color_2 (const ChafaColor *color, ChafaColorSpace color_space)
{
    gint best_error;
    gint best_index = CHAFA_PALETTE_INDEX_BLACK;
    const ChafaColor *palette_color;
    gint error;

    palette_color = chafa_get_palette_color_256 (CHAFA_PALETTE_INDEX_BLACK, color_space);
    best_error = chafa_color_diff_slow (color, palette_color, color_space);

    palette_color = chafa_get_palette_color_256 (CHAFA_PALETTE_INDEX_WHITE, color_space);
    error = chafa_color_diff_slow (color, palette_color, color_space);
    if (error < best_error)
        best_index = CHAFA_PALETTE_INDEX_WHITE;

    return best_index;
}
