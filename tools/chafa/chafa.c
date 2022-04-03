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

#include <stdio.h>
#include <string.h>  /* strspn, strlen, strcmp, strncmp, memset */
#include <locale.h>  /* setlocale */
#include <sys/ioctl.h>  /* ioctl */
#include <sys/types.h>  /* open */
#include <sys/stat.h>  /* stat */
#include <fcntl.h>  /* open */
#include <unistd.h>  /* STDOUT_FILENO */
#include <signal.h>  /* sigaction */
#include <termios.h>  /* tcgetattr, tcsetattr */

#include <chafa.h>
#include "font-loader.h"
#include "media-loader.h"
#include "named-colors.h"

#define ANIM_FPS_MAX 100000.0
#define FILE_DURATION_DEFAULT 0.0

typedef struct
{
    gchar *executable_name;

    gboolean show_help;
    gboolean show_version;

    GList *args;
    ChafaCanvasMode mode;
    ChafaColorExtractor color_extractor;
    ChafaColorSpace color_space;
    ChafaDitherMode dither_mode;
    ChafaPixelMode pixel_mode;
    gint dither_grain_width;
    gint dither_grain_height;
    gdouble dither_intensity;
    ChafaSymbolMap *symbol_map;
    ChafaSymbolMap *fill_symbol_map;
    gboolean symbols_specified;
    gboolean is_interactive;
    gboolean clear;
    gboolean verbose;
    gboolean invert;
    gboolean preprocess;
    gboolean polite;
    gboolean stretch;
    gboolean zoom;
    gboolean watch;
    gboolean fg_only;
    gboolean animate;
    gboolean center;
    gint width, height;
    gint cell_width, cell_height;
    gint margin_bottom, margin_right;
    gdouble font_ratio;
    gint work_factor;
    gint optimization_level;
    gint n_threads;
    ChafaOptimizations optimizations;
    guint32 fg_color;
    gboolean fg_color_set;
    guint32 bg_color;
    gboolean bg_color_set;
    gdouble transparency_threshold;
    gdouble file_duration_s;

    /* If > 0.0, override the framerate specified by the input file. */
    gdouble anim_fps;

    /* Applied after FPS is determined. If final value >= ANIM_FPS_MAX,
     * eliminate interframe delay altogether. */
    gdouble anim_speed_multiplier;

    /* Automatically set if terminal size is detected and there is
     * zero bottom margin. */
    gboolean have_parking_row;

    ChafaTermInfo *term_info;
}
GlobalOptions;

typedef struct
{
    gint width_cells, height_cells;
    gint width_pixels, height_pixels;
}
TermSize;

static GlobalOptions options;
static TermSize detected_term_size;
static volatile sig_atomic_t interrupted_by_user = FALSE;

static void
sigint_handler (G_GNUC_UNUSED int sig)
{
    interrupted_by_user = TRUE;
}

static void
interruptible_usleep (gint us)
{
    while (us > 0 && !interrupted_by_user)
    {
        gint sleep_us = MIN (us, 50000);
        g_usleep (sleep_us);
        us -= sleep_us;
    }
}

static gboolean
write_to_stdout (gconstpointer buf, gsize len)
{
    if (len == 0)
        return TRUE;

   return fwrite (buf, 1, len, stdout) == len ? TRUE : FALSE;
}

static guchar
get_hex_byte (const gchar *str)
{
    gint b = 0;

    if (*str >= '0' && *str <= '9')
        b = *str - '0';
    else if (*str >= 'a' && *str <= 'f')
        b = *str - 'a' + 10;
    else if (*str >= 'A' && *str <= 'F')
        b = *str - 'A' + 10;
    else
        g_assert_not_reached ();

    b <<= 4;
    str++;

    if (*str >= '0' && *str <= '9')
        b |= *str - '0';
    else if (*str >= 'a' && *str <= 'f')
        b |= *str - 'a' + 10;
    else if (*str >= 'A' && *str <= 'F')
        b |= *str - 'A' + 10;
    else
        g_assert_not_reached ();

    return b;
}

static gboolean
parse_color (const gchar *str, guint32 *col_out, GError **error)
{
    gint len;
    gchar *label = NULL;
    gchar *p0;
    gboolean result = FALSE;

    str += strspn (str, " \t");
    len = strspn (str, "#abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
    if (len < 1)
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Unrecognized color '%s'.", str);
        goto out;
    }

    p0 = label = g_ascii_strdown (str, len);

    /* Hex triplet, optionally prefixed with # or 0x */

    if (*p0 == '#') p0++;
    else if (p0 [0] == '0' && (p0 [1] == 'x')) p0 += 2;

    len = strspn (p0, "0123456789abcdef");
    if (len != (gint) strlen (p0) || len < 6)
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Unrecognized color '%s'.", str);
        goto out;
    }

    if (len > 6) p0 += (len - 6);

    *col_out = (get_hex_byte (p0) << 16) + (get_hex_byte (p0 + 2) << 8) + get_hex_byte (p0 + 4);

    result = TRUE;

out:
    g_free (label);
    return result;
}

static GList *
collect_variable_arguments (int *argc, char **argv [], gint first_arg)
{
    GList *l = NULL;
    gint i;

    for (i = first_arg; i < *argc; i++)
        l = g_list_prepend (l, g_strdup ((*argv) [i]));

    return g_list_reverse (l);
}

static const gchar copyright_notice [] =
    "Copyright (C) 2018-2022 Hans Petter Jansson et al.\n"
    "Incl. libnsgif copyright (C) 2004 Richard Wilson, copyright (C) 2008 Sean Fox\n"
    "Incl. LodePNG copyright (C) 2005-2018 Lode Vandevenne\n\n"
    "This is free software; see the source for copying conditions.  There is NO\n"
    "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n";

static void
print_version (void)
{
    gchar *builtin_features_str = chafa_describe_features (chafa_get_builtin_features ());
    gchar *supported_features_str = chafa_describe_features (chafa_get_supported_features ());

    g_print ("Chafa version %s\n\nFeatures: %s\nApplying: %s\n\n%s\n",
             CHAFA_VERSION,
             chafa_get_builtin_features () ? builtin_features_str : "none",
             chafa_get_supported_features () ? supported_features_str : "none",
             copyright_notice);

    g_free (builtin_features_str);
    g_free (supported_features_str);
}

static void
print_brief_summary (void)
{
    g_printerr ("%s: You must specify input files as arguments or pipe a file to stdin.\n"
                "Try '%s --help' for more information.\n",
                options.executable_name,
                options.executable_name);
}

static void
print_summary (void)
{
    const gchar *summary =
    "  Chafa (Character Art Facsimile) terminal graphics and character art generator.\n\n"

    "Options:\n"

    "  -h  --help         Show help.\n"
    "      --version      Show version.\n"
    "  -v, --verbose      Be verbose.\n\n"

    "      --animate=BOOL  Whether to allow animation [on, off]. Defaults to on.\n"
    "                     When off, will show a still frame from each animation.\n"
    "      --bg=COLOR     Background color of display (color name or hex).\n"
    "  -C, --center=BOOL  Center images [on, off]. Defaults to off.\n"
    "      --clear        Clear screen before processing each file.\n"
    "  -c, --colors=MODE  Set output color mode; one of [none, 2, 8, 16, 240, 256,\n"
    "                     full]. Defaults to full (24-bit).\n"
    "      --color-extractor=EXTR  Method for extracting color from an area\n"
    "                     [average, median]. Average is the default.\n"
    "      --color-space=CS  Color space used for quantization; one of [rgb, din99d].\n"
    "                     Defaults to rgb, which is faster but less accurate.\n"
    "      --dither=DITHER  Set output dither mode; one of [none, ordered,\n"
    "                     diffusion]. No effect with 24-bit color. Defaults to none.\n"
    "      --dither-grain=WxH  Set dimensions of dither grains in 1/8ths of a\n"
    "                     character cell [1, 2, 4, 8]. Defaults to 4x4.\n"
    "      --dither-intensity=NUM  Multiplier for dither intensity [0.0 - inf].\n"
    "                     Defaults to 1.0.\n"
    "  -d, --duration=SECONDS  The time to show each file. If showing a single file,\n"
    "                     defaults to zero for a still image and infinite for an\n"
    "                     animation. For multiple files, defaults to zero. Animations\n"
    "                     will always be played through at least once.\n"
    "      --fg=COLOR     Foreground color of display (color name or hex).\n"
    "      --fg-only      Leave the background color untouched. This produces\n"
    "                     character-cell output using foreground colors only.\n"
    "      --fill=SYMS    Specify character symbols to use for fill/gradients.\n"
    "                     Defaults to none. See below for full usage.\n"
    "      --font-ratio=W/H  Target font's width/height ratio. Can be specified as\n"
    "                     a real number or a fraction. Defaults to 1/2.\n"
    "  -f, --format=FORMAT  Set output format; one of [iterm, kitty, sixels,\n"
    "                     symbols]. Iterm, kitty and sixels yield much higher\n"
    "                     quality but enjoy limited support. Symbols mode yields\n"
    "                     beautiful character art.\n"
    "      --glyph-file=FILE  Load glyph information from FILE, which can be any\n"
    "                     font file supported by FreeType (TTF, PCF, etc).\n"
    "      --invert       Invert video. For display with bright backgrounds in\n"
    "                     color modes 2 and none. Swaps --fg and --bg.\n"
    "      --margin-bottom=NUM  When terminal size is detected, reserve at least NUM\n"
    "                     rows at the bottom as a safety margin. Can be used to\n"
    "                     prevent images from scrolling out. Defaults to 1.\n"
    "      --margin-right=NUM  When terminal size is detected, reserve at least NUM\n"
    "                     columns on the right-hand side as a safety margin. Defaults\n"
    "                     to 0.\n"
    "  -O, --optimize=NUM  Compress the output by using control sequences\n"
    "                     intelligently [0-9]. 0 disables, 9 enables every\n"
    "                     available optimization. Defaults to 5, except for when\n"
    "                     used with \"-c none\", where it defaults to 0.\n"
    "      --polite=BOOL  Polite mode [on, off]. Defaults to on. Turning this off\n"
    "                     may enhance presentation and prevent interference from\n"
    "                     other programs, but risks leaving the terminal in an\n"
    "                     altered state (rude).\n"
    "  -p, --preprocess=BOOL  Image preprocessing [on, off]. Defaults to on with 16\n"
    "                     colors or lower, off otherwise.\n"
    "  -s, --size=WxH     Set maximum output dimensions in columns and rows. By\n"
    "                     default this will be the size of your terminal, or 80x25\n"
    "                     if size detection fails.\n"
    "      --speed=SPEED  Set the speed animations will play at. This can be\n"
    "                     either a unitless multiplier, or a real number followed\n"
    "                     by \"fps\" to apply a specific framerate.\n"
    "      --stretch      Stretch image to fit output dimensions; ignore aspect.\n"
    "                     Implies --zoom.\n"
    "      --symbols=SYMS  Specify character symbols to employ in final output.\n"
    "                     See below for full usage and a list of symbol classes.\n"
    "      --threads=NUM  Maximum number of CPU threads to use. If left unspecified\n"
    "                     or negative, this will equal available CPU cores.\n"
    "  -t, --threshold=NUM  Threshold above which full transparency will be used\n"
    "                     [0.0 - 1.0].\n"
    "      --watch        Watch a single input file, redisplaying it whenever its\n"
    "                     contents change. Will run until manually interrupted\n"
    "                     or, if --duration is set, until it expires.\n"
    "  -w, --work=NUM     How hard to work in terms of CPU and memory [1-9]. 1 is the\n"
    "                     cheapest, 9 is the most accurate. Defaults to 5.\n"
    "      --zoom         Allow scaling up beyond one character per pixel.\n\n"

    "  Accepted classes for --symbols and --fill are [all, none, space, solid,\n"
    "  stipple, block, border, diagonal, dot, quad, half, hhalf, vhalf, inverted,\n"
    "  braille, technical, geometric, ascii, legacy, sextant, wedge, wide, narrow,\n"
    "  extra]. Some symbols belong to multiple classes, e.g. diagonals are also\n"
    "  borders. You can specify a list of classes separated by commas, or prefix them\n"
    "  with + and - to add or remove symbols relative to the existing set. The\n"
    "  ordering is significant.\n\n"

    "  The default symbol set is block+border+space-wide-inverted for all modes\n"
    "  except \"none\", which uses block+border+space-wide (including inverse symbols).\n\n"

    "Examples:\n"

    "  Generate 16-color output with perceptual color picking and avoid using\n"
    "  dot and stipple symbols:\n\n"
    "  $ chafa -c 16 --color-space din99d --symbols -dot-stipple in.jpg\n\n"

    "  Generate uncolored output using block and border symbols, but avoid the\n"
    "  solid block symbol:\n\n"
    "  $ chafa -c none --symbols block+border-solid in.png\n";

    g_print ("Usage:\n  %s [OPTION...] [FILE...]\n\n%s\n",
             options.executable_name,
             summary);
}

static gboolean
parse_colors_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result = TRUE;

    if (!g_ascii_strcasecmp (value, "none"))
        options.mode = CHAFA_CANVAS_MODE_FGBG;
    else if (!g_ascii_strcasecmp (value, "2"))
        options.mode = CHAFA_CANVAS_MODE_FGBG_BGFG;
    else if (!g_ascii_strcasecmp (value, "16"))
        options.mode = CHAFA_CANVAS_MODE_INDEXED_16;
    else if (!g_ascii_strcasecmp (value, "8"))
        options.mode = CHAFA_CANVAS_MODE_INDEXED_8;
    else if (!g_ascii_strcasecmp (value, "240"))
        options.mode = CHAFA_CANVAS_MODE_INDEXED_240;
    else if (!g_ascii_strcasecmp (value, "256"))
        options.mode = CHAFA_CANVAS_MODE_INDEXED_256;
    else if (!g_ascii_strcasecmp (value, "full")
             || !g_ascii_strcasecmp (value, "rgb")
             || !g_ascii_strcasecmp (value, "tc")
             || !g_ascii_strcasecmp (value, "truecolor"))
        options.mode = CHAFA_CANVAS_MODE_TRUECOLOR;
    else
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Colors must be one of [none, 2, 16, 240, 256, full].");
        result = FALSE;
    }

    return result;
}

static gboolean
parse_color_extractor_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result = TRUE;

    if (!g_ascii_strcasecmp (value, "average"))
        options.color_extractor = CHAFA_COLOR_EXTRACTOR_AVERAGE;
    else if (!g_ascii_strcasecmp (value, "median"))
        options.color_extractor = CHAFA_COLOR_EXTRACTOR_MEDIAN;
    else
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Color extractor must be one of [average, median].");
        result = FALSE;
    }

    return result;
}

static gboolean
parse_color_space_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result = TRUE;

    if (!g_ascii_strcasecmp (value, "rgb"))
        options.color_space = CHAFA_COLOR_SPACE_RGB;
    else if (!g_ascii_strcasecmp (value, "din99d"))
        options.color_space = CHAFA_COLOR_SPACE_DIN99D;
    else
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Color space must be one of [rgb, din99d].");
        result = FALSE;
    }

    return result;
}

static gboolean
parse_dither_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result = TRUE;

    if (!g_ascii_strcasecmp (value, "none"))
        options.dither_mode = CHAFA_DITHER_MODE_NONE;
    else if (!g_ascii_strcasecmp (value, "ordered")
             || !g_ascii_strcasecmp (value, "bayer"))
        options.dither_mode = CHAFA_DITHER_MODE_ORDERED;
    else if (!g_ascii_strcasecmp (value, "diffusion")
             || !g_ascii_strcasecmp (value, "fs"))
        options.dither_mode = CHAFA_DITHER_MODE_DIFFUSION;
    else
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Dither must be one of [none, ordered, diffusion].");
        result = FALSE;
    }

    return result;
}

static gboolean
parse_font_ratio_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result = TRUE;
    gdouble ratio = -1.0;
    gint width = -1, height = -1;
    gint o = 0;

    sscanf (value, "%d/%d%n", &width, &height, &o);
    if (width < 0 || height < 0)
        sscanf (value, "%d:%d%n", &width, &height, &o);
    if (width < 0 || height < 0)
        sscanf (value, "%lf%n", &ratio, &o);

    if (o != 0 && value [o] != '\0')
    {
        width = -1;
        height = -1;
        ratio = -1.0;
        goto out;
    }

    if (width > 0 && height > 0)
        ratio = width / (gdouble) height;

out:
    if (ratio <= 0.0)
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Font ratio must be specified as a real number or fraction.");
        result = FALSE;
    }
    else
    {
        options.font_ratio = ratio;
    }

    return result;
}

static gboolean
parse_symbols_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    options.symbols_specified = TRUE;
    return chafa_symbol_map_apply_selectors (options.symbol_map, value, error);
}

static gboolean
parse_fill_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    return chafa_symbol_map_apply_selectors (options.fill_symbol_map, value, error);
}

static gboolean
parse_format_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result = TRUE;
    ChafaPixelMode pixel_mode = CHAFA_PIXEL_MODE_SYMBOLS;

    if (!strcasecmp (value, "symbol") || !strcasecmp (value, "symbols")
        || !strcasecmp (value, "ansi"))
    {
        pixel_mode = CHAFA_PIXEL_MODE_SYMBOLS;
    }
    else if (!strcasecmp (value, "sixel") || !strcasecmp (value, "sixels"))
    {
        pixel_mode = CHAFA_PIXEL_MODE_SIXELS;
    }
    else if (!strcasecmp (value, "kitty"))
    {
        pixel_mode = CHAFA_PIXEL_MODE_KITTY;
    }
    else if (!strcasecmp (value, "iterm")
             || !strcasecmp (value, "iterm2"))
    {
        pixel_mode = CHAFA_PIXEL_MODE_ITERM2;
    }
    else
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Output format given as '%s'. Must be one of [iterm, kitty, sixels, symbols].",
                     value);
        result = FALSE;
    }

    if (result)
        options.pixel_mode = pixel_mode;

    return result;
}

static gboolean
parse_size_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result = TRUE;
    gint width = -1, height = -1;
    gint n;
    gint o = 0;

    n = sscanf (value, "%d%n", &width, &o);

    if (value [o] == 'x' && value [o + 1] != '\0')
    {
        gint o2;

        n = sscanf (value + o + 1, "%d%n", &height, &o2);
        if (n == 1 && value [o + o2 + 1] != '\0')
        {
            width = height = -1;
            goto out;
        }
    }

out:
    if (width < 0 && height < 0)
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Output dimensions must be specified as [width]x[height], [width]x or x[height], e.g 80x25, 80x or x25.");
        result = FALSE;
    }
    else if (width == 0 || height == 0)
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Output dimensions must be at least 1x1.");
        result = FALSE;
    }

    options.width = width;
    options.height = height;

    return result;
}

static gboolean
parse_dither_grain_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result = TRUE;
    gint width = -1, height = -1;
    gint n;
    gint o = -1;

    n = sscanf (value, "%d%n", &width, &o);

    if (o > 0 && value [o] == 'x' && value [o + 1] != '\0')
    {
        gint o2;

        n = sscanf (value + o + 1, "%d%n", &height, &o2);
        if (n == 1 && value [o + o2 + 1] != '\0')
        {
            width = height = -1;
            goto out;
        }
    }

out:
    if (height < 0)
        height = width;

    if (width < 0)
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Grain size must be specified as [width]x[height] or [dim], e.g. 8x4 or 4.");
        result = FALSE;
    }
    else if ((width != 1 && width != 2 && width != 4 && width != 8) ||
             (height != 1 && height != 2 && height != 4 && height != 8))
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Grain dimensions must be exactly 1, 2, 4 or 8.");
        result = FALSE;
    }

    options.dither_grain_width = width;
    options.dither_grain_height = height;

    return result;
}

static gboolean
parse_glyph_file_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result = FALSE;
    FileMapping *file_mapping;
    FontLoader *font_loader;
    gunichar c;
    gpointer c_bitmap;
    gint width, height;

    file_mapping = file_mapping_new (value);
    if (!file_mapping)
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Unable to open glyph file '%s'.", value);
        goto out;
    }

    font_loader = font_loader_new_from_mapping (file_mapping);
    file_mapping = NULL;  /* Font loader owns it now */

    if (!font_loader)
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Unable to load glyph file '%s'.", value);
        goto out;
    }

    while (font_loader_get_next_glyph (font_loader, &c, &c_bitmap, &width, &height))
    {
        chafa_symbol_map_add_glyph (options.symbol_map, c,
                                    CHAFA_PIXEL_RGBA8_PREMULTIPLIED, c_bitmap,
                                    width, height, width * 4);
        chafa_symbol_map_add_glyph (options.fill_symbol_map, c,
                                    CHAFA_PIXEL_RGBA8_PREMULTIPLIED, c_bitmap,
                                    width, height, width * 4);
        g_free (c_bitmap);
    }

    font_loader_destroy (font_loader);

    result = TRUE;

out:
    if (file_mapping)
        file_mapping_destroy (file_mapping);
    return result;
}

static gboolean
parse_boolean_token (const gchar *token, gboolean *value_out)
{
    gboolean success = FALSE;

    if (!g_ascii_strcasecmp (token, "on")
        || !g_ascii_strcasecmp (token, "yes"))
    {
        *value_out = TRUE;
    }
    else if (!g_ascii_strcasecmp (token, "off")
             || !g_ascii_strcasecmp (token, "no"))
    {
        *value_out = FALSE;
    }
    else
    {
        goto out;
    }

    success = TRUE;

out:
    return success;
}

static gboolean
parse_animate_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result;

    result = parse_boolean_token (value, &options.animate);
    if (!result)
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Animate mode must be one of [on, off].");

    return result;
}

static gboolean
parse_center_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result;

    result = parse_boolean_token (value, &options.center);
    if (!result)
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Centering mode must be one of [on, off].");

    return result;
}

static gboolean
parse_preprocess_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result;

    result = parse_boolean_token (value, &options.preprocess);
    if (!result)
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Preprocessing must be one of [on, off].");

    return result;
}

static gboolean
parse_polite_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result;

    result = parse_boolean_token (value, &options.polite);
    if (!result)
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Polite mode must be one of [on, off].");

    return result;
}

static gboolean
parse_anim_speed_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result = FALSE;
    gdouble d;

    if (!g_ascii_strcasecmp (value, "max")
        || !g_ascii_strcasecmp (value, "maximum"))
    {
        options.anim_fps = G_MAXDOUBLE;
        result = TRUE;
    }
    else
    {
        gchar *endptr;

        d = g_strtod (value, &endptr);
        if (endptr == value || d <= 0.0)
            goto out;

        while (g_ascii_isspace (*endptr))
            endptr++;

        if (!g_ascii_strcasecmp (endptr, "fps"))
        {
            options.anim_fps = d;
            result = TRUE;
        }
        else if (*endptr == '\0')
        {
            options.anim_speed_multiplier = d;
            result = TRUE;
        }

        /* If there's unknown junk at the end, fail. */
    }

out:
    if (!result)
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Animation speed must be either \"max\", a real multiplier, or a real framerate followed by \"fps\". It must be greater than zero.");

    return result;
}

static gboolean
parse_color_str (const gchar *value, guint32 *col_out, const gchar *error_message, GError **error)
{
    const NamedColor *named_color;
    guint32 col = 0x000000;
    gboolean result = TRUE;

    named_color = find_color_by_name (value);
    if (named_color)
    {
        col = (named_color->color [0] << 16)
            | (named_color->color [1] << 8)
            | (named_color->color [2]);
    }
    else if (!parse_color (value, &col, NULL))
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     error_message, value);
        result = FALSE;
    }

    if (result)
        *col_out = col;

    return result;
}

static gboolean
parse_fg_color_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    options.fg_color_set = parse_color_str (value, &options.fg_color, "Unrecognized foreground color '%s'.", error);
    return options.fg_color_set;
}

static gboolean
parse_bg_color_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    options.bg_color_set = parse_color_str (value, &options.bg_color, "Unrecognized background color '%s'.", error);
    return options.bg_color_set;
}

static void
get_tty_size (TermSize *term_size_out)
{
    struct winsize w;
    gboolean have_winsz = FALSE;

    term_size_out->width_cells
        = term_size_out->height_cells
        = term_size_out->width_pixels
        = term_size_out->height_pixels
        = -1;

    /* FIXME: Use tcgetwinsize() when it becomes more widely available.
     * See: https://www.austingroupbugs.net/view.php?id=1151#c3856 */

    if (ioctl (STDOUT_FILENO, TIOCGWINSZ, &w) >= 0
        || ioctl (STDERR_FILENO, TIOCGWINSZ, &w) >= 0
        || ioctl (STDIN_FILENO, TIOCGWINSZ, &w) >= 0)
        have_winsz = TRUE;

    if (!have_winsz)
    {
        const gchar *term_path;
        gint fd = -1;

        term_path = ctermid (NULL);
        if (term_path)
            fd = open (term_path, O_RDONLY);

        if (fd >= 0)
        {
            if (ioctl (fd, TIOCGWINSZ, &w) >= 0)
                have_winsz = TRUE;

            close (fd);
        }
    }

    if (!have_winsz)
        return;

    if (w.ws_col > 0)
        term_size_out->width_cells = w.ws_col;

    if (w.ws_row > 2)
        term_size_out->height_cells = w.ws_row;

    /* If .ws_xpixel and .ws_ypixel are filled out, we can calculate
     * aspect information for the font used. Sixel-capable terminals
     * like mlterm set these fields, but most others do not. */

    if (w.ws_xpixel > 0 && w.ws_ypixel > 0)
    {
        term_size_out->width_pixels = w.ws_xpixel;
        term_size_out->height_pixels = w.ws_ypixel;
    }
}

static struct termios saved_termios;

static void
tty_options_init (void)
{
    if (!options.polite)
    {
        gchar buf [CHAFA_TERM_SEQ_LENGTH_MAX];
        gchar *p0;

        if (options.is_interactive)
        {
            struct termios t;

            tcgetattr (STDIN_FILENO, &saved_termios);
            t = saved_termios;
            t.c_lflag &= ~ECHO;
            tcsetattr (STDIN_FILENO, TCSANOW, &t);
        }

        if (options.mode != CHAFA_CANVAS_MODE_FGBG)
        {
            p0 = chafa_term_info_emit_disable_cursor (options.term_info, buf);
            write_to_stdout (buf, p0 - buf);
        }

        /* Most terminals should have sixel scrolling enabled by default, so we're
         * not going to disable it again later. */
        if (options.pixel_mode == CHAFA_PIXEL_MODE_SIXELS)
        {
            p0 = chafa_term_info_emit_enable_sixel_scrolling (options.term_info, buf);
            write_to_stdout (buf, p0 - buf);
        }
    }
}

static void
tty_options_deinit (void)
{
    if (!options.polite)
    {
        if (options.mode != CHAFA_CANVAS_MODE_FGBG)
        {
            gchar buf [CHAFA_TERM_SEQ_LENGTH_MAX];
            gchar *p0;

            p0 = chafa_term_info_emit_enable_cursor (options.term_info, buf);
            write_to_stdout (buf, p0 - buf);
        }

        if (options.is_interactive)
        {
            tcsetattr (STDIN_FILENO, TCSANOW, &saved_termios);
        }
    }
}

static void
detect_terminal (ChafaTermInfo **term_info_out, ChafaCanvasMode *mode_out, ChafaPixelMode *pixel_mode_out)
{
    ChafaCanvasMode mode;
    ChafaPixelMode pixel_mode;
    ChafaTermInfo *term_info;
    ChafaTermInfo *fallback_info;
    gchar **envp;

    envp = g_get_environ ();
    term_info = chafa_term_db_detect (chafa_term_db_get_default (), envp);

    if (chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_SET_COLOR_FGBG_DIRECT)
        && chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_SET_COLOR_FG_DIRECT)
        && chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_SET_COLOR_BG_DIRECT))
        mode = CHAFA_CANVAS_MODE_TRUECOLOR;
    else if (chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_SET_COLOR_FGBG_256)
        && chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_SET_COLOR_FG_256)
        && chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_SET_COLOR_BG_256))
        mode = CHAFA_CANVAS_MODE_INDEXED_240;
    else if (chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_SET_COLOR_FGBG_16)
        && chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_SET_COLOR_FG_16)
        && chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_SET_COLOR_BG_16))
        mode = CHAFA_CANVAS_MODE_INDEXED_16;
    else if (chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_INVERT_COLORS)
             && chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_RESET_ATTRIBUTES))
        mode = CHAFA_CANVAS_MODE_FGBG_BGFG;
    else
        mode = CHAFA_CANVAS_MODE_FGBG;

    if (chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_BEGIN_KITTY_IMMEDIATE_IMAGE_V1))
        pixel_mode = CHAFA_PIXEL_MODE_KITTY;
    else if (chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_BEGIN_SIXELS)
             && chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_END_SIXELS))
        pixel_mode = CHAFA_PIXEL_MODE_SIXELS;
    else
        pixel_mode = CHAFA_PIXEL_MODE_SYMBOLS;

    /* Make sure we have fallback sequences in case the user forces
     * a mode that's technically unsupported by the terminal. */
    fallback_info = chafa_term_db_get_fallback_info (chafa_term_db_get_default ());
    chafa_term_info_supplement (term_info, fallback_info);
    chafa_term_info_unref (fallback_info);

    *term_info_out = term_info;
    *mode_out = mode;
    *pixel_mode_out = pixel_mode;

    g_strfreev (envp);
}

static gboolean
parse_options (int *argc, char **argv [])
{
    GError *error = NULL;
    GOptionContext *context;
    gboolean result = TRUE;
    const GOptionEntry option_entries [] =
    {
        /* Note: The descriptive blurbs here are never shown to the user */

        { "help",        'h',  0, G_OPTION_ARG_NONE,     &options.show_help,    "Show help", NULL },
        { "version",     '\0', 0, G_OPTION_ARG_NONE,     &options.show_version, "Show version", NULL },
        { "verbose",     'v',  0, G_OPTION_ARG_NONE,     &options.verbose,      "Be verbose", NULL },
        { "animate",     '\0', 0, G_OPTION_ARG_CALLBACK, parse_animate_arg,     "Animate", NULL },
        { "bg",          '\0', 0, G_OPTION_ARG_CALLBACK, parse_bg_color_arg,    "Background color of display", NULL },
        { "center",      'C',  0, G_OPTION_ARG_CALLBACK, parse_center_arg,      "Center", NULL },
        { "clear",       '\0', 0, G_OPTION_ARG_NONE,     &options.clear,        "Clear", NULL },
        { "colors",      'c',  0, G_OPTION_ARG_CALLBACK, parse_colors_arg,      "Colors (none, 2, 16, 256, 240 or full)", NULL },
        { "color-extractor", '\0', 0, G_OPTION_ARG_CALLBACK, parse_color_extractor_arg, "Color extractor (average or median)", NULL },
        { "color-space", '\0', 0, G_OPTION_ARG_CALLBACK, parse_color_space_arg, "Color space (rgb or din99d)", NULL },
        { "dither",      '\0', 0, G_OPTION_ARG_CALLBACK, parse_dither_arg,      "Dither", NULL },
        { "dither-grain",'\0', 0, G_OPTION_ARG_CALLBACK, parse_dither_grain_arg, "Dither grain", NULL },
        { "dither-intensity", '\0',  0, G_OPTION_ARG_DOUBLE,   &options.dither_intensity, "Dither intensity", NULL },
        { "duration",    'd',  0, G_OPTION_ARG_DOUBLE,   &options.file_duration_s, "Duration", NULL },
        { "fg",          '\0', 0, G_OPTION_ARG_CALLBACK, parse_fg_color_arg,    "Foreground color of display", NULL },
        { "fg-only",     '\0', 0, G_OPTION_ARG_NONE,     &options.fg_only,      "Foreground only", NULL },
        { "fill",        '\0', 0, G_OPTION_ARG_CALLBACK, parse_fill_arg,        "Fill symbols", NULL },
        { "format",      'f',  0, G_OPTION_ARG_CALLBACK, parse_format_arg,      "Format of output pixel data (iterm, kitty, sixels or symbols)", NULL },
        { "font-ratio",  '\0', 0, G_OPTION_ARG_CALLBACK, parse_font_ratio_arg,  "Font ratio", NULL },
        { "glyph-file",  '\0', 0, G_OPTION_ARG_CALLBACK, parse_glyph_file_arg,  "Glyph file", NULL },
        { "invert",      '\0', 0, G_OPTION_ARG_NONE,     &options.invert,       "Invert foreground/background", NULL },
        { "margin-bottom", '\0', 0, G_OPTION_ARG_INT,    &options.margin_bottom,  "Bottom margin", NULL },
        { "margin-right", '\0', 0, G_OPTION_ARG_INT,     &options.margin_right,  "Right margin", NULL },
        { "optimize",    'O',  0, G_OPTION_ARG_INT,      &options.optimization_level,  "Optimization", NULL },
        { "polite",      '\0', 0, G_OPTION_ARG_CALLBACK, parse_polite_arg,      "Polite", NULL },
        { "preprocess",  'p',  0, G_OPTION_ARG_CALLBACK, parse_preprocess_arg,  "Preprocessing", NULL },
        { "work",        'w',  0, G_OPTION_ARG_INT,      &options.work_factor,  "Work factor", NULL },
        { "size",        's',  0, G_OPTION_ARG_CALLBACK, parse_size_arg,        "Output size", NULL },
        { "speed",       '\0', 0, G_OPTION_ARG_CALLBACK, parse_anim_speed_arg,  "Animation speed", NULL },
        { "stretch",     '\0', 0, G_OPTION_ARG_NONE,     &options.stretch,      "Stretch image to fix output dimensions", NULL },
        { "symbols",     '\0', 0, G_OPTION_ARG_CALLBACK, parse_symbols_arg,     "Output symbols", NULL },
        { "threads",     '\0', 0, G_OPTION_ARG_INT,      &options.n_threads,    "Number of threads", NULL },
        { "threshold",   't',  0, G_OPTION_ARG_DOUBLE,   &options.transparency_threshold, "Transparency threshold", NULL },
        { "watch",       '\0', 0, G_OPTION_ARG_NONE,     &options.watch,        "Watch a file's contents", NULL },
        { "zoom",        '\0', 0, G_OPTION_ARG_NONE,     &options.zoom,         "Allow scaling up beyond one character per pixel", NULL },
        { NULL }
    };
    ChafaCanvasMode canvas_mode;
    ChafaPixelMode pixel_mode;
    gboolean using_detected_size = FALSE;

    memset (&options, 0, sizeof (options));

    context = g_option_context_new ("[COMMAND] [OPTION...]");
    g_option_context_set_help_enabled (context, FALSE);
    g_option_context_add_main_entries (context, option_entries, NULL);

    options.executable_name = g_strdup ((*argv) [0]);

    /* Defaults */

    options.symbol_map = chafa_symbol_map_new ();
    chafa_symbol_map_add_by_tags (options.symbol_map, CHAFA_SYMBOL_TAG_BLOCK);
    chafa_symbol_map_add_by_tags (options.symbol_map, CHAFA_SYMBOL_TAG_BORDER);
    chafa_symbol_map_add_by_tags (options.symbol_map, CHAFA_SYMBOL_TAG_SPACE);
    chafa_symbol_map_remove_by_tags (options.symbol_map, CHAFA_SYMBOL_TAG_WIDE);

    options.fill_symbol_map = chafa_symbol_map_new ();

    options.is_interactive = isatty (STDIN_FILENO) && isatty (STDOUT_FILENO);
    detect_terminal (&options.term_info, &canvas_mode, &pixel_mode);
    options.mode = CHAFA_CANVAS_MODE_MAX;  /* Unset */
    options.pixel_mode = pixel_mode;
    options.dither_mode = CHAFA_DITHER_MODE_NONE;
    options.dither_grain_width = -1;  /* Unset */
    options.dither_grain_height = -1;  /* Unset */
    options.dither_intensity = 1.0;
    options.animate = TRUE;
    options.center = FALSE;
    options.polite = TRUE;
    options.preprocess = TRUE;
    options.fg_only = FALSE;
    options.color_extractor = CHAFA_COLOR_EXTRACTOR_AVERAGE;
    options.color_space = CHAFA_COLOR_SPACE_RGB;
    options.width = -1;  /* Unset */
    options.height = -1;  /* Unset */
    options.font_ratio = -1.0;  /* Unset */
    options.margin_bottom = -1;  /* Unset */
    options.margin_right = -1;  /* Unset */
    options.work_factor = 5;
    options.optimization_level = G_MININT;  /* Unset */
    options.n_threads = -1;
    options.fg_color = 0xffffff;
    options.bg_color = 0x000000;
    options.transparency_threshold = G_MAXDOUBLE;  /* Unset */
    options.file_duration_s = G_MAXDOUBLE;
    options.anim_fps = -1.0;
    options.anim_speed_multiplier = 1.0;

    if (!g_option_context_parse (context, argc, argv, &error))
    {
        g_printerr ("%s: %s\n", options.executable_name, error->message);
        return FALSE;
    }

    /* Detect terminal geometry */

    get_tty_size (&detected_term_size);

    if (detected_term_size.width_cells > 0
        && detected_term_size.height_cells > 0
        && detected_term_size.width_pixels > 0
        && detected_term_size.height_pixels > 0)
    {
        options.cell_width = detected_term_size.width_pixels / detected_term_size.width_cells;
        options.cell_height = detected_term_size.height_pixels / detected_term_size.height_cells;

        if (options.font_ratio <= 0.0
            && options.cell_width > 0
            && options.cell_height > 0)
        {
            options.font_ratio = (gfloat) options.cell_width / (gfloat) options.cell_height;
        }
    }

    /* Assign detected or default dimensions if none specified */

    if (options.width < 0 && options.height < 0)
    {
        using_detected_size = TRUE;
    }

    if (options.margin_bottom < 0)
    {
        options.margin_bottom = 1;  /* Default */
    }

    /* Sixel output always leaves the cursor below the image, so force a bottom
     * margin of at least one even if the user wanted less. */
    if (options.margin_bottom < 1
        && options.pixel_mode == CHAFA_PIXEL_MODE_SIXELS)
        options.margin_bottom = 1;

    if (options.margin_right < 0)
    {
        options.margin_right = 0;  /* Default */

        /* Kitty leaves the cursor in the column trailing the last row of the
         * image. However, if the image is as wide as the terminal, the cursor
         * will wrap to the first column of the following row, making us lose
         * track of its position.
         *
         * This is not a problem when instructed to clear the terminal, as we
         * use absolute positioning then.
         *
         * If needed, trim one column from the image to make the cursor position
         * predictable. */
        if (options.pixel_mode == CHAFA_PIXEL_MODE_KITTY
            && using_detected_size
            && !(options.clear && options.margin_bottom >= 2))
        {
            options.margin_right = 1;
        }
    }

    if (using_detected_size)
    {
        options.width = detected_term_size.width_cells;
        options.height = detected_term_size.height_cells;

        if (options.width < 0 && options.height < 0)
        {
            options.width = 80;
            options.height = 25;
        }

        options.width = (options.width > options.margin_right)
            ? options.width - options.margin_right : 1;

        options.height = (options.height > options.margin_bottom)
            ? options.height - options.margin_bottom : 1;
    }

    options.have_parking_row = (using_detected_size && options.margin_bottom == 0) ? FALSE : TRUE;

    /* Now we've established the pixel mode, apply dependent defaults */

    if (options.pixel_mode == CHAFA_PIXEL_MODE_SYMBOLS)
    {
        /* Character cell defaults */

        if (options.mode == CHAFA_CANVAS_MODE_MAX)
            options.mode = canvas_mode;
        if (options.dither_grain_width < 0)
            options.dither_grain_width = 4;
        if (options.dither_grain_height < 0)
            options.dither_grain_height = 4;
        if (options.font_ratio <= 0.0)
            options.font_ratio = 1.0 / 2.0;
    }
    else
    {
        /* iTerm2/Kitty/sixel defaults */

        if (options.mode == CHAFA_CANVAS_MODE_MAX)
            options.mode = CHAFA_CANVAS_MODE_TRUECOLOR;
        if (options.dither_grain_width < 0)
            options.dither_grain_width = 1;
        if (options.dither_grain_height < 0)
            options.dither_grain_height = 1;
        if (options.font_ratio <= 0.0)
            options.font_ratio = 1.0 / 2.0;
    }

    if (options.work_factor < 1 || options.work_factor > 9)
    {
        g_printerr ("%s: Work factor must be in the range [1-9].\n", options.executable_name);
        return FALSE;
    }

    if (options.transparency_threshold == G_MAXDOUBLE)
        options.transparency_threshold = 0.5;

    if (options.transparency_threshold < 0.0 || options.transparency_threshold > 1.0)
    {
        g_printerr ("%s: Transparency threshold %.1lf is not in the range [0.0-1.0].\n", options.executable_name, options.transparency_threshold);
        return FALSE;
    }

    if (options.show_version)
    {
        print_version ();
        return TRUE;
    }

    if (*argc > 1)
    {
        options.args = collect_variable_arguments (argc, argv, 1);
    }
    else if (!isatty (STDIN_FILENO))
    {
        /* Receiving data through a pipe, and no file arguments. Act as if
         * invoked with "chafa -". */

        options.args = g_list_append (NULL, g_strdup ("-"));
    }
    else if (!options.show_help)
    {
        /* No arguments, no pipe, and no cry for help. */
        print_brief_summary ();
        return FALSE;
    }

    /* This is outside the if-else ladder above because we want it to happen even if
     * there are other arguments. */
    if (options.show_help)
    {
        print_summary ();
        return TRUE;
    }

    if (options.watch && g_list_length (options.args) != 1)
    {
        g_printerr ("%s: Can only use --watch with exactly one file.\n", options.executable_name);
        return FALSE;
    }

    /* --stretch implies --zoom */
    options.zoom |= options.stretch;

    if (options.invert)
    {
        guint32 temp_color;

        temp_color = options.bg_color;
        options.bg_color = options.fg_color;
        options.fg_color = temp_color;
    }

    if (options.file_duration_s == G_MAXDOUBLE
        && (!options.is_interactive
            || (options.args && options.args->next)))
    {
        /* Apply a zero default duration when we have multiple files or it looks
         * like we're part of a pipe; we don't want to get stuck if the user is
         * trying to e.g. batch convert files */
        options.file_duration_s = FILE_DURATION_DEFAULT;
    }

    /* Since FGBG mode can't use escape sequences to invert, it really
     * needs inverted symbols. In other modes they will only slow us down,
     * so disable them unless the user specified symbols of their own. */
    if (options.mode != CHAFA_CANVAS_MODE_FGBG && !options.symbols_specified)
        chafa_symbol_map_remove_by_tags (options.symbol_map, CHAFA_SYMBOL_TAG_INVERTED);

    /* If optimization level is unset, enable optimizations. However, we
     * leave them off for FGBG mode, since control sequences may be
     * unexpected in this mode unless explicitly asked for. */
    if (options.optimization_level == G_MININT)
        options.optimization_level = (options.mode == CHAFA_CANVAS_MODE_FGBG) ? 0 : 5;

    if (options.optimization_level < 0 || options.optimization_level > 9)
    {
        g_printerr ("%s: Optimization level %d is not in the range [0-9].\n",
                    options.executable_name, options.optimization_level);
        return FALSE;
    }

    /* Translate optimization level to flags */

    options.optimizations = CHAFA_OPTIMIZATION_NONE;

    if (options.optimization_level >= 1)
        options.optimizations |= CHAFA_OPTIMIZATION_REUSE_ATTRIBUTES;
    if (options.optimization_level >= 6)
        options.optimizations |= CHAFA_OPTIMIZATION_REPEAT_CELLS;
    if (options.optimization_level >= 7)
        options.optimizations |= CHAFA_OPTIMIZATION_SKIP_CELLS;

    chafa_set_n_threads (options.n_threads);

    g_option_context_free (context);

    return result;
}

#define PAD_SPACES_MAX 4096

static gboolean
write_pad_spaces (gint n)
{
    gchar buf [PAD_SPACES_MAX];
    gint i;

    g_assert (n >= 0);

    n = MIN (n, PAD_SPACES_MAX);
    for (i = 0; i < n; i++)
        buf [i] = ' ';

    return write_to_stdout (buf, n);
}

/* Write out the image data, possibly centering it */
static gboolean
write_image (const gchar *data, gsize len, gint dest_width)
{
    gint left_space;
    gboolean result = FALSE;

    left_space = options.center ? (detected_term_size.width_cells - dest_width) / 2 : 0;

    /* Indent top left corner: Common for all modes */
    if (left_space > 0)
    {
        if (!write_pad_spaces (left_space))
            goto out;
    }

    if (left_space <= 0 || options.pixel_mode != CHAFA_PIXEL_MODE_SYMBOLS)
    {
        if (!write_to_stdout (data, len))
            goto out;
    }
    else
    {
        const gchar *end, *p0, *p1;

        /* Indent subsequent rows: Symbols mode only */

        for (p0 = data, end = data + len; p0 < end; p0 = p1)
        {
            p1 = memchr (p0, '\n', end - p0);
            p1 = p1 ? (p1 + 1) : end;

            if (!write_to_stdout (p0, p1 - p0))
                goto out;

            if (p1 != end)
            {
                if (!write_pad_spaces (left_space))
                    goto out;
            }
        }
    }

    result = TRUE;

out:
    return result;
}

static GString *
build_string (ChafaPixelType pixel_type, const guint8 *pixels,
              gint src_width, gint src_height, gint src_rowstride,
              gint dest_width, gint dest_height)
{
    ChafaCanvasConfig *config;
    ChafaCanvas *canvas;
    GString *gs;

    config = chafa_canvas_config_new ();

    chafa_canvas_config_set_geometry (config, dest_width, dest_height);
    chafa_canvas_config_set_canvas_mode (config, options.mode);
    chafa_canvas_config_set_pixel_mode (config, options.pixel_mode);
    chafa_canvas_config_set_dither_mode (config, options.dither_mode);
    chafa_canvas_config_set_dither_grain_size (config, options.dither_grain_width, options.dither_grain_height);
    chafa_canvas_config_set_dither_intensity (config, options.dither_intensity);
    chafa_canvas_config_set_color_extractor (config, options.color_extractor);
    chafa_canvas_config_set_color_space (config, options.color_space);
    chafa_canvas_config_set_fg_color (config, options.fg_color);
    chafa_canvas_config_set_bg_color (config, options.bg_color);
    chafa_canvas_config_set_preprocessing_enabled (config, options.preprocess);
    chafa_canvas_config_set_fg_only_enabled (config, options.fg_only);

    if (options.transparency_threshold >= 0.0)
        chafa_canvas_config_set_transparency_threshold (config, options.transparency_threshold);
    if (options.cell_width > 0 && options.cell_height > 0)
        chafa_canvas_config_set_cell_geometry (config, options.cell_width, options.cell_height);

    chafa_canvas_config_set_symbol_map (config, options.symbol_map);
    chafa_canvas_config_set_fill_symbol_map (config, options.fill_symbol_map);

    /* Work switch takes values [1..9], we normalize to [0.0..1.0] to
     * get the work factor. */
    chafa_canvas_config_set_work_factor (config, (options.work_factor - 1) / 8.0f);

    chafa_canvas_config_set_optimizations (config, options.optimizations);

    canvas = chafa_canvas_new (config);
    chafa_canvas_draw_all_pixels (canvas, pixel_type, pixels, src_width, src_height, src_rowstride);
    gs = chafa_canvas_print (canvas, options.term_info);

    chafa_canvas_unref (canvas);
    chafa_canvas_config_unref (config);
    return gs;
}

static gboolean
run_generic (const gchar *filename, gboolean is_first_file, gboolean is_first_frame, gboolean quiet)
{
    gboolean is_animation = FALSE;
    gdouble anim_elapsed_s = 0.0;
    GTimer *timer;
    gint loop_n = 0;
    MediaLoader *media_loader;
    gchar buf [CHAFA_TERM_SEQ_LENGTH_MAX * 2 + 3];
    GString *gs;
    gchar *p0;

    timer = g_timer_new ();

    media_loader = media_loader_new (filename);
    if (!media_loader)
    {
        if (!quiet)
            g_printerr ("%s: Failed to open '%s'.\n",
                        options.executable_name,
                        filename);
        goto out;
    }

    if (interrupted_by_user)
        goto out;

    is_animation = options.animate ? media_loader_get_is_animation (media_loader) : FALSE;

    do
    {
        gboolean have_frame;

        /* Outer loop repeats animation if desired */

        media_loader_goto_first_frame (media_loader);

        for (have_frame = TRUE;
             (is_first_frame || is_animation)
             && have_frame && !interrupted_by_user && (loop_n == 0 || anim_elapsed_s < options.file_duration_s);
             have_frame = media_loader_goto_next_frame (media_loader))
        {
            gdouble elapsed_ms, remain_ms;
            gint delay_ms;
            ChafaPixelType pixel_type;
            gint src_width, src_height, src_rowstride;
            gint dest_width, dest_height;
            const guint8 *pixels;

            g_timer_start (timer);

            pixels = media_loader_get_frame_data (media_loader,
                                                  &pixel_type,
                                                  &src_width,
                                                  &src_height,
                                                  &src_rowstride);
            /* FIXME: This shouldn't happen -- but if it does, our
             * options for handling it gracefully here aren't great.
             * Needs refactoring. */
            if (!pixels)
                break;

            delay_ms = media_loader_get_frame_delay (media_loader);

            dest_width = options.width;
            dest_height = options.height;

            chafa_calc_canvas_geometry (src_width,
                                        src_height,
                                        &dest_width,
                                        &dest_height,
                                        options.font_ratio,
                                        options.zoom,
                                        options.stretch);

            gs = build_string (pixel_type, pixels,
                               src_width, src_height, src_rowstride,
                               dest_width, dest_height);

            p0 = buf;

            if (options.clear)
            {
                if (is_first_frame)
                {
                    /* Clear */
                    p0 = chafa_term_info_emit_clear (options.term_info, p0);
                }

                /* Home cursor between frames */
                p0 = chafa_term_info_emit_cursor_to_top_left (options.term_info, p0);
            }
            else if (!is_first_frame)
            {
                /* Cursor to col 0 and up N steps */
                *(p0++) = '\r';
                p0 = chafa_term_info_emit_cursor_up (options.term_info, p0, dest_height - (options.have_parking_row ? 0 : 1));
            }

            /* Put a blank line between files in non-clear mode */
            if (is_first_frame && !options.clear && !is_first_file)
            {
                if (!options.have_parking_row)
                    *(p0++) = '\n';
                *(p0++) = '\n';
            }

            if (!write_to_stdout (buf, p0 - buf))
                goto out;

            if (!write_image (gs->str, gs->len, dest_width))
                goto out;

            g_string_free (gs, TRUE);

            /* No linefeed after frame in sixel mode */
            if (options.have_parking_row
                && (options.pixel_mode == CHAFA_PIXEL_MODE_SYMBOLS
                    || options.pixel_mode == CHAFA_PIXEL_MODE_KITTY
                    || options.pixel_mode == CHAFA_PIXEL_MODE_ITERM2))
            {
                if (!write_to_stdout ("\n", 1))
                    goto out;
            }
            else if (options.center && options.pixel_mode == CHAFA_PIXEL_MODE_SIXELS)
            {
                /* If image was centered in sixel mode, cursor must be brought
                 * back to left margin manually */
                if (!write_to_stdout ("\r", 1))
                    goto out;
            }

            if (fflush (stdout) != 0)
                goto out;

            if (is_animation)
            {
                /* Account for time spent converting and printing frame */
                elapsed_ms = g_timer_elapsed (timer, NULL) * 1000.0;

                if (options.anim_fps > 0.0)
                    remain_ms = 1000.0 / options.anim_fps;
                else
                    remain_ms = delay_ms;
                remain_ms /= options.anim_speed_multiplier;
                remain_ms = MAX (remain_ms - elapsed_ms, 0);

                if (remain_ms > 0.0001 && 1000.0 / (gdouble) remain_ms < ANIM_FPS_MAX)
                    interruptible_usleep (remain_ms * 1000);

                anim_elapsed_s += MAX (elapsed_ms, delay_ms) / 1000.0;
            }

            is_first_frame = FALSE;
        }

        loop_n++;
    }
    while (is_animation && !interrupted_by_user
           && !options.watch && anim_elapsed_s < options.file_duration_s);

out:
    if (media_loader)
        media_loader_destroy (media_loader);
    g_timer_destroy (timer);

    return is_animation;
}

static gboolean
run (const gchar *filename, gboolean is_first_file, gboolean is_first_frame, gboolean quiet)
{
    return run_generic (filename, is_first_file, is_first_frame, quiet);
}

static int
run_watch (const gchar *filename)
{
    GTimer *timer;
    gboolean is_first_frame = TRUE;

    tty_options_init ();
    timer = g_timer_new ();

    for ( ; !interrupted_by_user; )
    {
        struct stat sbuf;

        if (!stat (filename, &sbuf))
        {
            /* Sadly we can't rely on timestamps to tell us when to reload
             * the file, since they can take way too long to update. */

            run (filename, TRUE, is_first_frame, TRUE);
            is_first_frame = FALSE;

            g_usleep (10000);
        }
        else
        {
            /* Don't hammer the path if the file is temporarily gone */

            g_usleep (250000);
        }

        if (g_timer_elapsed (timer, NULL) > options.file_duration_s)
            break;
    }

    g_timer_destroy (timer);
    tty_options_deinit ();
    return 0;
}

static int
run_all (GList *filenames)
{
    GList *l;

    if (!filenames)
        return 0;

    tty_options_init ();

    for (l = filenames; l && !interrupted_by_user; l = g_list_next (l))
    {
        gchar *filename = l->data;
        gboolean was_animation;

        was_animation = run (filename, l->prev ? FALSE : TRUE, TRUE, FALSE);

        if (!was_animation && options.file_duration_s != G_MAXDOUBLE)
        {
            interruptible_usleep (options.file_duration_s * 1000000.0);
        }
    }

    /* Emit linefeed after last image when cursor was not in parking row */
    if (!options.have_parking_row)
        write_to_stdout ("\n", 1);

    tty_options_deinit ();
    return 0;
}

static void
proc_init (void)
{
    struct sigaction sa = { 0 };

    /* Must do this early. Buffer size probably doesn't matter */
    setvbuf (stdout, NULL, _IOFBF, 32768);

    setlocale (LC_ALL, "");

    sa.sa_handler = sigint_handler;
    sa.sa_flags = SA_RESETHAND;

    sigaction (SIGINT, &sa, NULL);
}

int
main (int argc, char *argv [])
{
    int ret;

    proc_init ();

    if (!parse_options (&argc, &argv))
        exit (1);

    ret = options.watch
        ? run_watch (options.args->data)
        : run_all (options.args);

    if (options.symbol_map)
        chafa_symbol_map_unref (options.symbol_map);
    if (options.fill_symbol_map)
        chafa_symbol_map_unref (options.fill_symbol_map);
    if (options.term_info)
        chafa_term_info_unref (options.term_info);
    return ret;
}
