/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2025 Hans Petter Jansson
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

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>  /* strspn, strlen, strcmp, strncmp, memset */
#include <locale.h>  /* setlocale */
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>  /* ioctl */
#endif
#include <sys/types.h>  /* open */
#include <sys/stat.h>  /* stat */
#include <fcntl.h>  /* open */
#include <unistd.h>  /* STDOUT_FILENO */
#ifdef HAVE_SIGACTION
# include <signal.h>  /* sigaction */
#endif
#include <stdlib.h>  /* exit */
#ifdef HAVE_TERMIOS_H
# include <termios.h>  /* tcgetattr, tcsetattr */
#endif

#include <glib/gstdio.h>

#include <chafa.h>
#include "chafa-term.h"
#include "chicle-font-loader.h"
#include "chicle-grid-layout.h"
#include "chicle-media-pipeline.h"
#include "chicle-options.h"
#include "chicle-path-queue.h"
#include "chicle-placement-counter.h"
#include "chicle-util.h"

/* Include after glib.h for G_OS_WIN32 */
#ifdef G_OS_WIN32
# ifdef HAVE_WINDOWS_H
#  include <windows.h>
# endif
# include <wchar.h>
# include <io.h>
#endif

ChicleOptions options;
ChicleTermSize detected_term_size;
gboolean using_detected_size = FALSE;
ChiclePathQueue *global_path_queue;
gint global_path_queue_n_stdin;
gint global_n_path_streams;
ChafaTerm *term;

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

static gint
count_dash_strings (GList *l)
{
    gint n = 0;

    for ( ; l; l = g_list_next (l))
    {
        if (!strcmp (l->data, "-"))
            n++;
    }

    return n;
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
    "Copyright (C) 2018-2025 Hans Petter Jansson et al.\n"
    "Incl. libnsgif copyright (C) 2004 Richard Wilson, copyright (C) 2008 Sean Fox\n"
    "Incl. LodePNG copyright (C) 2005-2018 Lode Vandevenne\n"
    "Incl. QOI decoder copyright (C) 2021 Dominic Szablewski\n\n"
    "This is free software; see the source for copying conditions. There is NO\n"
    "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n";

static void
print_version (void)
{
    gchar *builtin_features_str = chafa_describe_features (chafa_get_builtin_features ());
    gchar *supported_features_str = chafa_describe_features (chafa_get_supported_features ());
    gchar **loaders = chicle_get_loader_names ();
    gchar *loaders_joined = g_strjoinv (" ", loaders);

    g_print ("Chafa version %s\n\nLoaders:  %s\nFeatures: %s\nApplying: %s\n\n%s\n",
             CHAFA_VERSION,
             loaders_joined,
             chafa_get_builtin_features () ? builtin_features_str : "none",
             chafa_get_supported_features () ? supported_features_str : "none",
             copyright_notice);

    g_free (builtin_features_str);
    g_free (supported_features_str);
    g_strfreev (loaders);
    g_free (loaders_joined);
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
    "  Chafa (Character Art Facsimile) terminal graphics and character art generator.\n"

    "\nGeneral options:\n"

    "      --files=PATH   Read list of files to process from PATH, or \"-\" for stdin.\n"
    "      --files0=PATH  Similar to --files, using NUL separator instead of newline.\n"
    "  -h, --help         Show help.\n"
    "      --probe=ARG    Probe terminal's capabilities and wait for response [auto,\n"
    "                     on, off]. A positive real number denotes the maximum time\n"
    "                     to wait for a response, in seconds. Defaults to "
                          G_STRINGIFY (CHICLE_PROBE_DURATION_DEFAULT) ".\n"
    "      --version      Show version.\n"
    "  -v, --verbose      Be verbose.\n"

    "\nOutput encoding:\n"

    "  -f, --format=FORMAT  Set output format; one of [iterm, kitty, sixels,\n"
    "                     symbols]. Iterm, kitty and sixels yield much higher\n"
    "                     quality but enjoy limited support. Symbols mode yields\n"
    "                     beautiful character art.\n"
    "  -O, --optimize=NUM  Compress the output by using control sequences\n"
    "                     intelligently [0-9]. 0 disables, 9 enables every\n"
    "                     available optimization. Defaults to 5, except for when\n"
    "                     used with \"-c none\", where it defaults to 0.\n"
    "      --relative=BOOL  Use relative cursor positioning [on, off]. When on,\n"
    "                     control sequences will be used to position images relative\n"
    "                     to the cursor. When off, newlines will be used to separate\n"
    "                     rows instead for e.g. 'less -R' interop. Defaults to off.\n"
    "      --passthrough=MODE  Graphics protocol passthrough [auto, none, screen,\n"
    "                     tmux]. Used to show pixel graphics through multiplexers.\n"
    "      --polite=BOOL  Polite mode [on, off]. Inhibits escape sequences that may\n"
    "                     confuse other programs. Defaults to off.\n"

    "\nSize and layout:\n"

    "      --align=ALIGN  Horizontal and vertical alignment (e.g. \"top,left\").\n"
    "      --clear        Clear screen before processing each file.\n"
    "      --exact-size=MODE  Try to match the input's size exactly [auto, on, off].\n"
    "      --fit-width    Fit images to view's width, possibly exceeding its height.\n"
    "      --font-ratio=W/H  Target font's width/height ratio. Can be specified as\n"
    "                     a real number or a fraction. Defaults to 1/2.\n"
    "      --grid=CxR     Lay out images in a grid of CxR columns/rows per screenful.\n"
    "                     C or R may be omitted, e.g. \"--grid 4\". Can be \"auto\".\n"
    "  -g                 Alias for \"--grid auto\".\n"
    "      --label=BOOL   Labeling [on, off]. Filenames below images. Default off.\n"
    "  -l                 Alias for \"--label on\".\n"
    "      --link=BOOL    Link labels [auto, on, off]. Turns labels into clickable\n"
    "                     hyperlinks. Use with \"-l on\". Defaults to auto.\n"
    "      --margin-bottom=NUM  When terminal size is detected, reserve at least NUM\n"
    "                     rows at the bottom as a safety margin. Can be used to\n"
    "                     prevent images from scrolling out. Defaults to 1.\n"
    "      --margin-right=NUM  When terminal size is detected, reserve at least NUM\n"
    "                     columns safety margin on right-hand side. Defaults to 0.\n"
    "      --scale=NUM    Scale image, respecting view's dimensions. 1.0 approximates\n"
    "                     image's pixel dimensions. Specify \"max\" to fit view.\n"
    "                     Defaults to 1.0 for pixel graphics and 4.0 for symbols.\n"
    "  -s, --size=WxH     Set maximum image dimensions in columns and rows. By\n"
    "                     default this will be equal to the view size.\n"
    "      --stretch      Stretch image to fit output dimensions; ignore aspect.\n"
    "                     Implies --scale max.\n"
    "      --view-size=WxH  Set the view size in columns and rows. By default this\n"
    "                     will be the size of your terminal, or 80x25 if size\n"
    "                     detection fails. If one dimension is omitted, it will\n"
    "                     be set to a reasonable approximation of infinity.\n"

    "\nAnimation and timing:\n"

    "      --animate=BOOL  Whether to allow animation [on, off]. Defaults to on.\n"
    "                     When off, will show a still frame from each animation.\n"
    "  -d, --duration=SECONDS  How long to show each file. If showing a single file,\n"
    "                     defaults to zero for a still image and infinite for an\n"
    "                     animation. For multiple files, defaults to zero. Animations\n"
    "                     will always be played through at least once.\n"
    "      --speed=SPEED  Animation speed. Either a unitless multiplier, or a real\n"
    "                     number followed by \"fps\" to apply a specific framerate.\n"
    "      --watch        Watch a single input file, redisplaying it whenever its\n"
    "                     contents change. Will run until manually interrupted\n"
    "                     or, if --duration is set, until it expires.\n"

    "\nColors and processing:\n"

    "      --bg=COLOR     Background color of display (color name or hex).\n"
    "  -c, --colors=MODE  Set output color mode; one of [none, 2, 8, 16/8, 16, 240,\n"
    "                     256, full]. Defaults to best guess.\n"
    "      --color-extractor=EXTR  Method for extracting color from an area\n"
    "                     [average, median]. Average is the default.\n"
    "      --color-space=CS  Color space used for quantization; one of [rgb, din99d].\n"
    "                     Defaults to rgb, which is faster but less accurate.\n"
    "      --dither=DITHER  Set output dither mode; one of [none, ordered,\n"
    "                     diffusion, noise]. No effect with 24-bit color. Defaults to\n"
    "                     noise for sixels, none otherwise.\n"
    "      --dither-grain=WxH  Set dimensions of dither grains in 1/8ths of a\n"
    "                     character cell [1, 2, 4, 8]. Defaults to 4x4.\n"
    "      --dither-intensity=NUM  Multiplier for dither intensity [0.0 - inf].\n"
    "                     Defaults to 1.0.\n"
    "      --fg=COLOR     Foreground color of display (color name or hex).\n"
    "      --invert       Swaps --fg and --bg. Useful with light terminal background.\n"
    "  -p, --preprocess=BOOL  Image preprocessing [on, off]. Defaults to on with 16\n"
    "                     colors or lower, off otherwise.\n"
    "  -t, --threshold=NUM  Lower threshold for full transparency [0.0 - 1.0].\n"

    "\nResource allocation:\n"

    "      --threads=NUM  Maximum number of CPU threads to use. If left unspecified\n"
    "                     or negative, this will equal available CPU cores.\n"
    "  -w, --work=NUM     How hard to work in terms of CPU and memory [1-9]. 1 is the\n"
    "                     cheapest, 9 is the most accurate. Defaults to 5.\n"

    "\nExtra options for symbol encoding:\n"

    "      --fg-only      Leave the background color untouched. This produces\n"
    "                     character-cell output using foreground colors only.\n"
    "      --fill=SYMS    Specify character symbols to use for fill/gradients.\n"
    "                     Defaults to none. See below for full usage.\n"
    "      --glyph-file=FILE  Load glyph information from FILE, which can be any\n"
    "                     font file supported by FreeType (TTF, PCF, etc).\n"
    "      --symbols=SYMS  Specify character symbols to employ in final output.\n"
    "                     See below for full usage and a list of symbol classes.\n"

    "\nAccepted classes for --symbols and --fill:\n"

    "  all        ascii   braille   extra      imported  narrow   solid      ugly\n"
    "  alnum      bad     diagonal  geometric  inverted  none     space      vhalf\n"
    "  alpha      block   digit     half       latin     quad     stipple    wedge\n"
    "  ambiguous  border  dot       hhalf      legacy    sextant  technical  wide\n"

    "\n  These can be combined with + and -, e.g. block+border-diagonal or all-wide.\n"

    "\nExamples:\n"

    "  $ chafa --scale max in.jpg                           # As big as will fit\n"
    "  $ chafa --clear --align mid,mid -d 5 *.gif           # Centered slideshow\n"
    "  $ chafa -f symbols --symbols ascii -c none in.png    # Old-school ASCII art\n"
    "  $ find /usr -type f -print0 | chafa --files0 - -g -l # System images (Unix)\n\n"

    "If your OS comes with manual pages, you can type 'man chafa' for more.\n";

    g_print ("Usage:\n  %s [OPTION...] [FILE...]\n\n%s",
             options.executable_name,
             summary);
}

static gboolean
parse_boolean_token (const gchar *token, gboolean *value_out)
{
    gboolean success = FALSE;

    if (!g_ascii_strcasecmp (token, "on")
        || !g_ascii_strcasecmp (token, "yes")
        || !g_ascii_strcasecmp (token, "true"))
    {
        *value_out = TRUE;
    }
    else if (!g_ascii_strcasecmp (token, "off")
             || !g_ascii_strcasecmp (token, "no")
             || !g_ascii_strcasecmp (token, "false"))
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
parse_tristate_token (const gchar *token, ChicleTristate *value_out)
{
    gboolean success = FALSE;
    gboolean value_bool = FALSE;

    if (!g_ascii_strcasecmp (token, "auto")
        || !g_ascii_strcasecmp (token, "default"))
    {
        *value_out = CHICLE_TRISTATE_AUTO;
    }
    else
    {
        success = parse_boolean_token (token, &value_bool);
        if (!success)
            goto out;

        *value_out = value_bool ? CHICLE_TRISTATE_TRUE : CHICLE_TRISTATE_FALSE;
    }

    success = TRUE;

out:
    return success;
}

static gboolean
parse_probe_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result = FALSE;

    result = parse_tristate_token (value, &options.probe);
    if (!result)
    {
        gdouble d = -1.0;
        gchar *endptr;

        d = g_strtod (value, &endptr);
        if (endptr == value || d <= 0.0)
            goto out;

        options.probe = CHICLE_TRISTATE_TRUE;
        options.probe_duration = d;
        result = TRUE;
    }

out:
    if (!result)
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Probe duration must be a positive real number or one of [on, off, auto].");

    return result;
}

static gboolean
parse_colors_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result = TRUE;

    if (!g_ascii_strcasecmp (value, "none"))
        options.mode = CHAFA_CANVAS_MODE_FGBG;
    else if (!g_ascii_strcasecmp (value, "2"))
        options.mode = CHAFA_CANVAS_MODE_FGBG_BGFG;
    else if (!g_ascii_strcasecmp (value, "8"))
        options.mode = CHAFA_CANVAS_MODE_INDEXED_8;
    else if (!g_ascii_strcasecmp (value, "16-8")
             || !g_ascii_strcasecmp (value, "16/8"))
        options.mode = CHAFA_CANVAS_MODE_INDEXED_16_8;
    else if (!g_ascii_strcasecmp (value, "16"))
        options.mode = CHAFA_CANVAS_MODE_INDEXED_16;
    else if (!g_ascii_strcasecmp (value, "240"))
        options.mode = CHAFA_CANVAS_MODE_INDEXED_240;
    else if (!g_ascii_strcasecmp (value, "256"))
        options.mode = CHAFA_CANVAS_MODE_INDEXED_256;
    else if (!g_ascii_strcasecmp (value, "full")
             || !g_ascii_strcasecmp (value, "rgb")
             || !g_ascii_strcasecmp (value, "tc")
             || !g_ascii_strcasecmp (value, "direct")
             || !g_ascii_strcasecmp (value, "directcolor")
             || !g_ascii_strcasecmp (value, "truecolor"))
        options.mode = CHAFA_CANVAS_MODE_TRUECOLOR;
    else
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Colors must be one of [none, 2, 8, 16/8, 16, 240, 256, full].");
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
    else if (!g_ascii_strcasecmp (value, "noise"))
        options.dither_mode = CHAFA_DITHER_MODE_NOISE;
    else
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Dither must be one of [none, ordered, diffusion, noise].");
        result = FALSE;
    }

    options.dither_mode_set = TRUE;
    return result;
}

static const gchar *
utf8_skip_spaces (const gchar *str)
{
    gunichar c;

    for (c = g_utf8_get_char (str);
         c != 0 && g_unichar_isspace (c);
         c = g_utf8_get_char (str))
    {
        str = g_utf8_next_char (str);
    }

    return str;
}

static gboolean
parse_fraction_or_real (const gchar *str, gdouble *real_out)
{
    gboolean success = FALSE;
    gdouble ratio = G_MAXDOUBLE;
    gint64 width = -1, height = -1;
    const gchar *sep;
    const gchar *p0 = NULL;
    const gchar *end = NULL;

    p0 = utf8_skip_spaces (str);

    sep = g_utf8_strchr (p0, -1, '/');
    if (!sep)
        sep = g_utf8_strchr (p0, -1, ':');

    if (sep)
    {
        width = g_ascii_strtoll (p0, (gchar **)(intptr_t) &end, 10);
        if (!end || end == p0)
            goto out;
        end = utf8_skip_spaces (end);
        if (g_utf8_get_char (end) != '/' && g_utf8_get_char (end) != ':')
            goto out;

        sep = g_utf8_next_char (sep);
        p0 = utf8_skip_spaces (sep);
        height = g_ascii_strtoll (p0, (gchar **)(intptr_t) &end, 10);
        if (end == p0)
            goto out;
        end = utf8_skip_spaces (end);
        if (!end || *end)
            goto out;

        if ((gdouble) height != 0.0)
            ratio = width / (gdouble) height;
    }
    else
    {
        ratio = g_strtod (p0, (gchar **)(intptr_t) &end);
        if (!end || end == p0)
            goto out;
        end = utf8_skip_spaces (end);
        if (!end || *end)
            goto out;
    }

    if (ratio == G_MAXDOUBLE)
        goto out;

    *real_out = ratio;
    success = TRUE;

out:
    return success;
}

static gboolean
parse_align_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean success = FALSE;
    gchar **tokens = NULL;
    ChafaAlign halign = CHAFA_ALIGN_MAX;
    ChafaAlign valign = CHAFA_ALIGN_MAX;
    gint i;

    tokens = g_strsplit (value, ",", -1);

    for (i = 0; tokens [i]; i++)
    {
        gchar *t = tokens [i];

        if (i >= 2)
        {
            g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                         "Too many alignment specifiers in \"%s\". Must be at most two.",
                         value);
            goto out;
        }

        if (!strcasecmp (t, "left"))
            halign = CHAFA_ALIGN_START;
        else if (!strcasecmp (t, "right"))
            halign = CHAFA_ALIGN_END;
        else if (!strcasecmp (t, "hcenter") || !strcasecmp (t, "hmid"))
            halign = CHAFA_ALIGN_CENTER;
        else if (!strcasecmp (t, "top") || !strcasecmp (t, "up"))
            valign = CHAFA_ALIGN_START;
        else if (!strcasecmp (t, "bottom") || !strcasecmp (t, "down"))
            valign = CHAFA_ALIGN_END;
        else if (!strcasecmp (t, "vcenter") || !strcasecmp (t, "vmid"))
            valign = CHAFA_ALIGN_CENTER;
        else if (!strcasecmp (t, "center") || !strcasecmp (t, "mid"))
            ;  /* Skip */
        else
        {
            g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                         "Unknown alignment specifier \"%s\".", t);
            goto out;
        }
    }

    /* Process "center" in a second pass to allow both e.g. "center,left"
     * and "left,center" */

    for (i = 0; tokens [i]; i++)
    {
        gchar *t = tokens [i];

        if (!strcasecmp (t, "center") || !strcasecmp (t, "mid"))
        {
            /* Allow "center,center" to center in both dimensions */
            if (halign == CHAFA_ALIGN_MAX)
                halign = CHAFA_ALIGN_CENTER;
            else
                valign = CHAFA_ALIGN_CENTER;
        }
    }

    if (halign != CHAFA_ALIGN_MAX)
        options.horiz_align = halign;
    if (valign != CHAFA_ALIGN_MAX)
        options.vert_align = valign;

    success = TRUE;

out:
    g_strfreev (tokens);
    return success;
}

static gboolean
parse_font_ratio_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gdouble ratio = -1.0;
    gboolean success = FALSE;

    if (!parse_fraction_or_real (value, &ratio) || ratio <= 0.0)
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Font ratio must be specified as a positive real number or fraction.");
        goto out;
    }

    options.font_ratio = ratio;
    success = TRUE;

out:
    return success;
}

static gboolean
parse_scale_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gdouble scale = -1.0;
    gboolean success = FALSE;

    if (!strcasecmp (value, "max") || !strcasecmp (value, "fill"))
    {
        scale = CHICLE_SCALE_MAX;
    }
    else if (!parse_fraction_or_real (value, &scale) || scale <= 0.0)
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Scale must be specified as a positive real number or fraction.");
        goto out;
    }

    options.scale = scale;
    success = TRUE;

out:
    return success;
}

static gboolean
parse_threshold_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gdouble threshold = -1.0;
    gboolean success = FALSE;

    if (!parse_fraction_or_real (value, &threshold) || threshold < 0.0 || threshold > 1.0)
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Opacity threshold must be a real number or fraction in the range [0.0-1.0].");
        goto out;
    }

    options.transparency_threshold = threshold;
    success = TRUE;

out:
    return success;
}

static gboolean
parse_dither_intensity_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gdouble dither_intensity = -1.0;
    gboolean success = FALSE;

    if (!parse_fraction_or_real (value, &dither_intensity) || dither_intensity < 0.0)
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Dither intensity must be a positive real number or fraction.");
        goto out;
    }

    options.dither_intensity = dither_intensity;
    success = TRUE;

out:
    return success;
}

static gboolean
parse_duration_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gdouble duration = -1.0;
    gboolean success = FALSE;

    if (!strcasecmp (value, "max")
        || !strcasecmp (value, "inf")
        || !strcasecmp (value, "infinite"))
    {
        duration = G_MAXDOUBLE;
    }
    else if (!parse_fraction_or_real (value, &duration) || duration < 0.0)
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Duration must be a positive real number or fraction, \"inf\" or \"infinite\".");
        goto out;
    }

    options.file_duration_s = duration;
    success = TRUE;

out:
    return success;
}

static gboolean
parse_symbols_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    options.symbols_specified = TRUE;

    /* FIXME: Implement a more convenient/consistent way to turn off Unicode in
     * labels etc. */
    if (value && !strcasecmp (value, "ascii"))
        options.use_unicode = FALSE;

    return chafa_symbol_map_apply_selectors (options.symbol_map, value, error);
}

static gboolean
parse_fill_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    return chafa_symbol_map_apply_selectors (options.fill_symbol_map, value, error);
}

static gboolean
parse_files_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, G_GNUC_UNUSED GError **error)
{
    global_n_path_streams++;

    if (!strcmp (value, "-"))
        global_path_queue_n_stdin++;

    chicle_path_queue_push_stream (global_path_queue, value, "\n", 1);
    return TRUE;
}

static gboolean
parse_files0_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, G_GNUC_UNUSED GError **error)
{
    global_n_path_streams++;

    if (!strcmp (value, "-"))
        global_path_queue_n_stdin++;

    chicle_path_queue_push_stream (global_path_queue, value, "\0", 1);
    return TRUE;
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
    {
        options.pixel_mode = pixel_mode;
        options.pixel_mode_set = TRUE;
    }

    return result;
}

static void
parse_2d_size (const gchar *value, gint *width_out, gint *height_out)
{
    gint n;
    gint o = 0;

    *width_out = *height_out = -1;
    n = sscanf (value, "%d%n", width_out, &o);

    if (value [o] == 'x' && value [o + 1] != '\0')
    {
        gint o2;

        n = sscanf (value + o + 1, "%d%n", height_out, &o2);
        if (n == 1 && value [o + o2 + 1] != '\0')
        {
            *width_out = *height_out = -1;
        }
    }
}

static gboolean
parse_view_size_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result = TRUE;
    gint width, height;

    parse_2d_size (value, &width, &height);

    if (width < 0 && height < 0)
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "View size must be specified as [width]x[height], [width]x or x[height], e.g 80x25, 80x or x25.");
        result = FALSE;
    }
    else if (width == 0 || height == 0)
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "View size must be at least 1x1.");
        result = FALSE;
    }

    options.view_width = width;
    options.view_height = height;

    return result;
}

static gboolean
parse_size_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result = TRUE;
    gint width, height;

    parse_2d_size (value, &width, &height);

    if (width < 0 && height < 0)
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Size must be specified as [width]x[height], [width]x or x[height], e.g 80x25, 80x or x25.");
        result = FALSE;
    }
    else if (width == 0 || height == 0)
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Size must be at least 1x1.");
        result = FALSE;
    }

    options.width = width;
    options.height = height;

    return result;
}

static gboolean
parse_grid_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result = TRUE;
    gboolean b = FALSE;
    gint width, height;

    if (parse_boolean_token (value, &b))
    {
        width = height = (b ? CHICLE_GRID_AUTO : -1);
        options.grid_on = b;
    }
    else if (!g_ascii_strcasecmp (value, "auto"))
    {
        width = height = CHICLE_GRID_AUTO;
        options.grid_on = TRUE;
    }
    else
    {
        parse_2d_size (value, &width, &height);

        if (width < 0 && height < 0)
        {
            g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                         "Grid size must be specified as [width]x[height], [width]x or x[height], e.g 4x4, 4x or x4.");
            result = FALSE;
        }
        else if (width == 0 || height == 0)
        {
            g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                         "Grid size must be at least 1x1.");
            result = FALSE;
        }
        else if (width < 0)
        {
            width = -1;
            options.grid_on = TRUE;
        }
        else if (height < 0)
        {
            height = -1;
            options.grid_on = TRUE;
        }
    }

    options.grid_width = width;
    options.grid_height = height;

    return result;
}

static gboolean
parse_exact_size_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result;

    result = parse_tristate_token (value, &options.use_exact_size);
    if (!result)
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Exact size selector must be one of [on, off, auto].");
    }

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
parse_passthrough_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result = TRUE;

    options.passthrough_set = TRUE;

    if (!g_ascii_strcasecmp (value, "none"))
        options.passthrough = CHAFA_PASSTHROUGH_NONE;
    else if (!g_ascii_strcasecmp (value, "screen"))
        options.passthrough = CHAFA_PASSTHROUGH_SCREEN;
    else if (!g_ascii_strcasecmp (value, "tmux"))
        options.passthrough = CHAFA_PASSTHROUGH_TMUX;
    else if (!g_ascii_strcasecmp (value, "auto"))
    {
        options.passthrough = CHAFA_PASSTHROUGH_NONE;
        options.passthrough_set = FALSE;
    }
    else
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Passthrough must be one of [auto, none, screen, tmux].");
        result = FALSE;
    }

    return result;
}

static gboolean
parse_glyph_file_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result = FALSE;
    ChicleFileMapping *file_mapping;
    ChicleFontLoader *font_loader;
    gunichar c;
    gpointer c_bitmap;
    gint width, height;

    file_mapping = chicle_file_mapping_new (value);
    if (!file_mapping)
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Unable to open glyph file '%s'.", value);
        goto out;
    }

    font_loader = chicle_font_loader_new_from_mapping (file_mapping);
    file_mapping = NULL;  /* Font loader owns it now */

    if (!font_loader)
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Unable to load glyph file '%s'.", value);
        goto out;
    }

    while (chicle_font_loader_get_next_glyph (font_loader, &c, &c_bitmap, &width, &height))
    {
        chafa_symbol_map_add_glyph (options.symbol_map, c,
                                    CHAFA_PIXEL_RGBA8_PREMULTIPLIED, c_bitmap,
                                    width, height, width * 4);
        chafa_symbol_map_add_glyph (options.fill_symbol_map, c,
                                    CHAFA_PIXEL_RGBA8_PREMULTIPLIED, c_bitmap,
                                    width, height, width * 4);
        g_free (c_bitmap);
    }

    chicle_font_loader_destroy (font_loader);

    result = TRUE;

out:
    if (file_mapping)
        chicle_file_mapping_destroy (file_mapping);
    return result;
}

static void
dump_glyph (gunichar c, const guint8 *pix, gint width, gint height, gint rowstride)
{
    gint x, y;
    const guint8 *p;
    gchar buf [16];
    gint len;

    len = g_unichar_to_utf8 (c, buf);
    g_assert (len >= 1 && len < 16);
    buf [len] = '\0';
    fprintf (stdout,
             "    {\n"
             "        /* [%s] */\n"
             "        CHAFA_SYMBOL_TAG_,\n"
             "        0x%x,\n"
             "        CHAFA_SYMBOL_OUTLINE_%s (",
             buf,
             c,
             (width == 8 && height == 8) ? "8X8" :
             (width == 16 && height == 8) ? "16X8" :
             "STRANGE_SIZE");

    for (y = 0; y < height; y++)
    {
        p = pix + y * rowstride;

        fputs ("\n            \"", stdout);

        for (x = 0; x < width; x++)
        {
            fputc (*p < 0x80 ? ' ' : 'X' , stdout);
            p += 4;
        }

        fputs ("\"", stdout);
    }

    fputs (")\n    },\n", stdout);
}

/* Undocumented functionality; dumps a font file so the glyphs can be tweaked and turned
 * into builtins. */
static gboolean
parse_dump_glyph_file_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result = FALSE;
    ChicleFileMapping *file_mapping;
    ChicleFontLoader *font_loader;
    ChafaSymbolMap *temp_map = NULL;
    gunichar c;
    gpointer c_bitmap;
    gint width, height, rowstride;

    options.skip_processing = TRUE;

    file_mapping = chicle_file_mapping_new (value);
    if (!file_mapping)
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Unable to open glyph file '%s'.", value);
        goto out;
    }

    font_loader = chicle_font_loader_new_from_mapping (file_mapping);
    file_mapping = NULL;  /* Font loader owns it now */

    if (!font_loader)
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Unable to load glyph file '%s'.", value);
        goto out;
    }

    temp_map = chafa_symbol_map_new ();

    while (chicle_font_loader_get_next_glyph (font_loader, &c, &c_bitmap, &width, &height))
    {
        gpointer pix;

        /* By adding and then querying the glyph, we get the benefit of Chafa's
         * internal scaling and postprocessing. */

        chafa_symbol_map_add_glyph (temp_map, c,
                                    CHAFA_PIXEL_RGBA8_PREMULTIPLIED, c_bitmap,
                                    width, height, width * 4);
        g_free (c_bitmap);

        chafa_symbol_map_get_glyph (temp_map, c,
                                    CHAFA_PIXEL_RGBA8_PREMULTIPLIED, &pix,
                                    &width, &height, &rowstride);
        dump_glyph (c, pix, width, height, rowstride);
        g_free (pix);
    }

    chicle_font_loader_destroy (font_loader);

    result = TRUE;

out:
    if (file_mapping)
        chicle_file_mapping_destroy (file_mapping);
    if (temp_map)
        chafa_symbol_map_unref (temp_map);
    return result;
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
    gboolean center = FALSE;
    gboolean result;

    result = parse_boolean_token (value, &center);
    if (result)
        options.horiz_align = center ? CHAFA_ALIGN_CENTER : CHAFA_ALIGN_START;
    else
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Centering mode must be one of [on, off].");

    return result;
}

static gboolean
parse_label_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result;

    result = parse_boolean_token (value, &options.label);
    if (!result)
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Label mode must be one of [on, off].");

    return result;
}

static gboolean
parse_link_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result;

    result = parse_tristate_token (value, &options.link_labels);
    if (!result)
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Link mode must be one of [auto, on, off].");

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
parse_relative_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result;

    result = parse_boolean_token (value, &options.relative);
    if (!result)
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Relative positioning must be one of [on, off].");

    options.relative_set = TRUE;
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
    const ChicleNamedColor *named_color;
    guint32 col = 0x000000;
    gboolean result = TRUE;

    named_color = chicle_find_color_by_name (value);
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

/* ---------------------------------- *
 * Option fuzzing with --fuzz-options *
 * ---------------------------------- */

#define FUZZ_SEED_LEN 150

static gboolean
fuzz_seed_get_bool (gconstpointer seed, gint seed_len, gint *ofs)
{
    const guchar *seed_u8 = seed;

    return seed_u8 [(*ofs)++ % seed_len] < 128 ? TRUE : FALSE;
}

static ChicleTristate
fuzz_seed_get_tristate (gconstpointer seed, gint seed_len, gint *ofs)
{
    const guchar *seed_u8 = seed;
    guint v = seed_u8 [(*ofs)++ % seed_len];

    return (v < 256 / 3) ? CHICLE_TRISTATE_FALSE
        : (v < (256 * 2) / 3) ? CHICLE_TRISTATE_TRUE
        : CHICLE_TRISTATE_AUTO;
}

static guint32
fuzz_seed_get_u32 (gconstpointer seed, gint seed_len, gint *ofs)
{
    const guchar *seed_u8 = seed;
    guint32 u32 = 0;
    gint i;

    for (i = 0; i < 4; i++)
    {
        u32 |= seed_u8 [(*ofs)++ % seed_len];
        u32 <<= 8;
    }

    return u32;
}

static guint32
fuzz_seed_get_uint (gconstpointer seed, gint seed_len, gint *ofs,
                    guint32 min, guint32 max)
{
    guint32 u32;

    u32 = fuzz_seed_get_u32 (seed, seed_len, ofs);
    return min + (u32 % (max - min));
}

static gdouble
fuzz_seed_get_double (gconstpointer seed, gint seed_len, gint *ofs,
                      gdouble min, gdouble max)
{
    guint32 u32;

    u32 = fuzz_seed_get_u32 (seed, seed_len, ofs);
    return min + (u32 % 65536) * ((max - min) / 65535.0);
}

static void
fuzz_options_with_seed (ChicleOptions *opt, gconstpointer seed, gint seed_len)
{
    gint ofs = 0;

    if (seed_len < 1)
        return;

    /* Fuzz as many options as possible, but avoid those needed to control
     * the fuzzing process -- e.g. n_threads, duration, animation speed, watch. */

    opt->mode = fuzz_seed_get_uint (seed, seed_len, &ofs, 0, CHAFA_CANVAS_MODE_MAX);
    opt->color_extractor = fuzz_seed_get_uint (seed, seed_len, &ofs, 0, CHAFA_COLOR_EXTRACTOR_MAX);
    opt->color_space = fuzz_seed_get_uint (seed, seed_len, &ofs, 0, CHAFA_COLOR_SPACE_MAX);
    opt->dither_mode = fuzz_seed_get_uint (seed, seed_len, &ofs, 0, CHAFA_DITHER_MODE_MAX);
    opt->dither_mode_set = TRUE;
    opt->pixel_mode = fuzz_seed_get_uint (seed, seed_len, &ofs, 0, CHAFA_PIXEL_MODE_MAX);
    opt->pixel_mode_set = TRUE;
    opt->dither_grain_width = 1 << (fuzz_seed_get_uint (seed, seed_len, &ofs, 0, 4));
    opt->dither_grain_height = 1 << (fuzz_seed_get_uint (seed, seed_len, &ofs, 0, 4));
    opt->dither_intensity = fuzz_seed_get_double (seed, seed_len, &ofs, 0.0, 10.0);
    opt->clear = fuzz_seed_get_bool (seed, seed_len, &ofs);
    opt->verbose = fuzz_seed_get_bool (seed, seed_len, &ofs);
    opt->invert = fuzz_seed_get_bool (seed, seed_len, &ofs);
    opt->preprocess = fuzz_seed_get_bool (seed, seed_len, &ofs);
    opt->polite = fuzz_seed_get_bool (seed, seed_len, &ofs);
    opt->stretch = fuzz_seed_get_bool (seed, seed_len, &ofs);
    opt->zoom = fuzz_seed_get_bool (seed, seed_len, &ofs);
    opt->fg_only = fuzz_seed_get_bool (seed, seed_len, &ofs);
    opt->animate = fuzz_seed_get_bool (seed, seed_len, &ofs);
    opt->horiz_align = fuzz_seed_get_uint (seed, seed_len, &ofs, 0, 3);
    opt->vert_align = fuzz_seed_get_uint (seed, seed_len, &ofs, 0, 3);
    opt->relative = fuzz_seed_get_bool (seed, seed_len, &ofs);
    opt->relative_set = TRUE;
    opt->fit_to_width = fuzz_seed_get_bool (seed, seed_len, &ofs);
    opt->view_width = fuzz_seed_get_uint (seed, seed_len, &ofs, 1, 32);
    opt->view_height = fuzz_seed_get_uint (seed, seed_len, &ofs, 1, 32);
    opt->width = fuzz_seed_get_uint (seed, seed_len, &ofs, 1, 32);
    opt->height = fuzz_seed_get_uint (seed, seed_len, &ofs, 1, 32);
    opt->cell_width = fuzz_seed_get_uint (seed, seed_len, &ofs, 1, 32);
    opt->cell_height = fuzz_seed_get_uint (seed, seed_len, &ofs, 1, 32);
    opt->margin_bottom = fuzz_seed_get_uint (seed, seed_len, &ofs, 0, 16);
    opt->margin_right = fuzz_seed_get_uint (seed, seed_len, &ofs, 0, 16);
    opt->scale = fuzz_seed_get_double (seed, seed_len, &ofs, 0.0, 10000.0);
    opt->work_factor = fuzz_seed_get_uint (seed, seed_len, &ofs, 1, 10);
    opt->optimization_level = fuzz_seed_get_uint (seed, seed_len, &ofs, 0, 10);
    opt->passthrough = fuzz_seed_get_uint (seed, seed_len, &ofs, 0, CHAFA_PASSTHROUGH_MAX);
    opt->passthrough_set = TRUE;
    opt->transparency_threshold = fuzz_seed_get_double (seed, seed_len, &ofs, 0.0, 1.0);
    opt->transparency_threshold_set = TRUE;
    opt->use_exact_size = fuzz_seed_get_tristate (seed, seed_len, &ofs);
}

static void
fuzz_options_with_file (ChicleOptions *opt, const gchar *filename)
{
    FILE *fhd;
    guchar seed [FUZZ_SEED_LEN];
    gint seed_len;

    fhd = fopen (filename, "rb");
    if (!fhd)
        return;

    /* Read seed from the file's tail to avoid correlation with image
     * format headers etc. */
    fseek (fhd, -FUZZ_SEED_LEN, SEEK_END);
    seed_len = fread (seed, sizeof (guchar), FUZZ_SEED_LEN, fhd);
    fclose (fhd);

    fuzz_options_with_seed (opt, seed, seed_len);
}

static gint
get_tmux_version (gchar **envp)
{
    const gchar *tmux_ver_str;
    const gchar *sep;

    tmux_ver_str = g_environ_getenv (envp, "TERM_PROGRAM_VERSION");
    if (!tmux_ver_str)
        return 0;

    sep = strchr (tmux_ver_str, '.');

    return g_ascii_strtoull (tmux_ver_str, NULL, 10) * 1000
        + (sep ? g_ascii_strtoull (sep + 1, NULL, 10) : 0);
}

static void
dump_settings (ChafaTermInfo *term_info, ChafaCanvasMode canvas_mode,
               ChafaPixelMode pixel_mode, ChafaPassthrough passthrough,
               gboolean polite)
{
    const gchar *canvas_mode_desc [] =
    {
        "truecolor",
        "indexed-256",
        "indexed-240",
        "indexed-16",
        "fgbg-bgfg",
        "fgbg",
        "indexed-8",
        "indexed-16-8",
        NULL
    };
    const gchar *pixel_mode_desc [] =
    {
        "symbols",
        "sixels",
        "kitty",
        "iterm2"
    };
    const gchar *passthrough_desc [] =
    {
        "none",
        "screen",
        "tmux"
    };

    g_print ("CHAFA_TERM='%s'\n"
             "CHAFA_CANVAS_MODE='%s'\n"
             "CHAFA_PIXEL_MODE='%s'\n"
             "CHAFA_PASSTHROUGH='%s'\n"
             "CHAFA_POLITE='%s'\n",
             chafa_term_info_get_name (term_info),
             canvas_mode_desc [canvas_mode],
             pixel_mode_desc [pixel_mode],
             passthrough_desc [passthrough],
             polite ? "true" : "false");
}

static void
detect_terminal (gchar **envp,
                 ChafaTermInfo **term_info_out, ChafaCanvasMode *mode_out,
                 ChafaPixelMode *pixel_mode_out, ChafaPassthrough *passthrough_out,
                 ChafaSymbolMap **symbol_map_out, ChafaSymbolMap **fill_symbol_map_out,
                 gboolean *polite_out)
{
    ChafaCanvasMode mode;
    ChafaPixelMode pixel_mode;
    ChafaPassthrough passthrough;
    ChafaTermInfo *term_info;
    ChafaTermInfo *fallback_info;

    term_info = chafa_term_db_detect (chafa_term_db_get_default (), envp);

    /* Let's see what this baby can do! */
    mode = chafa_term_info_get_best_canvas_mode (term_info);
    pixel_mode = chafa_term_info_get_best_pixel_mode (term_info);
    passthrough = chafa_term_info_get_is_pixel_passthrough_needed (term_info, pixel_mode)
        ? chafa_term_info_get_passthrough_type (term_info)
        : CHAFA_PASSTHROUGH_NONE;

    *symbol_map_out = chafa_symbol_map_new ();
    chafa_symbol_map_add_by_tags (*symbol_map_out,
                                  chafa_term_info_get_safe_symbol_tags (term_info));

    *fill_symbol_map_out = chafa_symbol_map_new ();

    /* Make sure we have fallback sequences in case the user forces
     * a mode that's technically unsupported by the terminal. */
    fallback_info = chafa_term_db_get_fallback_info (chafa_term_db_get_default ());
    chafa_term_info_supplement (term_info, fallback_info);
    chafa_term_info_unref (fallback_info);

    *term_info_out = term_info;
    *mode_out = mode;
    *pixel_mode_out = pixel_mode;
    *passthrough_out = passthrough;

    /* The 'lf' file browser will choke if there are extra sequences in front
     * of a sixel image. Let's be polite to it. */
    *polite_out = g_environ_getenv (envp, "LF_LEVEL") ? TRUE : FALSE;
}

static gchar *tmux_allow_passthrough_original;
static gboolean tmux_allow_passthrough_is_changed;

gboolean
chicle_apply_passthrough_workarounds_tmux (void)
{
    gboolean result = FALSE;
    gchar *standard_output = NULL;
    gchar *standard_error = NULL;
    gchar **strings;
    gchar *mode = NULL;
    gint wait_status = -1;

    /* allow-passthrough can be either unset, "on" or "all". Both "on" and "all"
     * are fine, so don't mess with it if we don't have to.
     *
     * Also note that we may be in a remote session inside tmux. */

    if (!g_spawn_command_line_sync ("tmux show allow-passthrough",
                                    &standard_output, &standard_error,
                                    &wait_status, NULL))
        goto out;

    strings = g_strsplit_set (standard_output, " ", -1);
    if (strings [0] && strings [1])
    {
        mode = g_ascii_strdown (strings [1], -1);
        g_strstrip (mode);
    }
    g_strfreev (strings);

    g_free (standard_output);
    standard_output = NULL;
    g_free (standard_error);
    standard_error = NULL;

    if (!mode || (strcmp (mode, "on") && strcmp (mode, "all")))
    {
        result = g_spawn_command_line_sync ("tmux set-option allow-passthrough on",
                                            &standard_output, &standard_error,
                                            &wait_status, NULL);
        if (result)
        {
            tmux_allow_passthrough_original = mode;
            tmux_allow_passthrough_is_changed = TRUE;
        }
    }
    else
    {
        g_free (mode);
    }

out:
    g_free (standard_output);
    g_free (standard_error);
    return result;
}

gboolean
chicle_retire_passthrough_workarounds_tmux (void)
{
    gboolean result = FALSE;
    gchar *standard_output = NULL;
    gchar *standard_error = NULL;
    gchar *cmd = NULL;
    gint wait_status = -1;

    if (!tmux_allow_passthrough_is_changed)
        return TRUE;

    if (tmux_allow_passthrough_original)
    {
        cmd = g_strdup_printf ("tmux set-option allow-passthrough %s",
                               tmux_allow_passthrough_original);
    }
    else
    {
        cmd = g_strdup ("tmux set-option -u allow-passthrough");
    }

    result = g_spawn_command_line_sync (cmd, &standard_output, &standard_error,
                                        &wait_status, NULL);
    if (result)
    {
        g_free (tmux_allow_passthrough_original);
        tmux_allow_passthrough_original = NULL;
        tmux_allow_passthrough_is_changed = FALSE;
    }

    g_free (cmd);
    g_free (standard_output);
    g_free (standard_error);
    return result;
}

gboolean
chicle_parse_options (int *argc, char **argv [])
{
    GError *error = NULL;
    GOptionContext *context;
    gboolean result = FALSE;
    const GOptionEntry option_entries [] =
    {
        /* Note: The descriptive blurbs here are never shown to the user */

        { "help",        'h',  0, G_OPTION_ARG_NONE,     &options.show_help,    "Show help", NULL },
        { "version",     '\0', 0, G_OPTION_ARG_NONE,     &options.show_version, "Show version", NULL },
        { "verbose",     'v',  0, G_OPTION_ARG_NONE,     &options.verbose,      "Be verbose", NULL },
        { "align",       '\0', 0, G_OPTION_ARG_CALLBACK, parse_align_arg,       "Align", NULL },
        { "animate",     '\0', 0, G_OPTION_ARG_CALLBACK, parse_animate_arg,     "Animate", NULL },
        { "bg",          '\0', 0, G_OPTION_ARG_CALLBACK, parse_bg_color_arg,    "Background color of display", NULL },
        { "center",      'C',  0, G_OPTION_ARG_CALLBACK, parse_center_arg,      "Center", NULL },
        { "clear",       '\0', 0, G_OPTION_ARG_NONE,     &options.clear,        "Clear", NULL },
        { "colors",      'c',  0, G_OPTION_ARG_CALLBACK, parse_colors_arg,      "Colors (none, 2, 16, 256, 240 or full)", NULL },
        { "color-extractor", '\0', 0, G_OPTION_ARG_CALLBACK, parse_color_extractor_arg, "Color extractor (average or median)", NULL },
        { "color-space", '\0', 0, G_OPTION_ARG_CALLBACK, parse_color_space_arg, "Color space (rgb or din99d)", NULL },
        { "dither",      '\0', 0, G_OPTION_ARG_CALLBACK, parse_dither_arg,      "Dither", NULL },
        { "dither-grain",'\0', 0, G_OPTION_ARG_CALLBACK, parse_dither_grain_arg, "Dither grain", NULL },
        { "dither-intensity", '\0', 0, G_OPTION_ARG_CALLBACK, parse_dither_intensity_arg, "Dither intensity", NULL },
        { "dump-detect", '\0', 0, G_OPTION_ARG_NONE,     &options.do_dump_detect, "Dump detection results", NULL },
        { "dump-glyph-file", '\0', 0, G_OPTION_ARG_CALLBACK, parse_dump_glyph_file_arg, "Dump glyph file", NULL },
        { "duration",    'd',  0, G_OPTION_ARG_CALLBACK, parse_duration_arg,    "Duration", NULL },
        { "exact-size",  '\0', 0, G_OPTION_ARG_CALLBACK, parse_exact_size_arg,  "Whether to prefer the original image size", NULL },
        { "fg",          '\0', 0, G_OPTION_ARG_CALLBACK, parse_fg_color_arg,    "Foreground color of display", NULL },
        { "fg-only",     '\0', 0, G_OPTION_ARG_NONE,     &options.fg_only,      "Foreground only", NULL },
        { "fill",        '\0', 0, G_OPTION_ARG_CALLBACK, parse_fill_arg,        "Fill symbols", NULL },
        { "files",       '\0', 0, G_OPTION_ARG_CALLBACK, parse_files_arg,       "File list", NULL },
        { "files0",      '\0', 0, G_OPTION_ARG_CALLBACK, parse_files0_arg,      "File list (NUL-separated)", NULL },
        { "fit-width",   '\0', 0, G_OPTION_ARG_NONE,     &options.fit_to_width, "Fit to width", NULL },
        { "format",      'f',  0, G_OPTION_ARG_CALLBACK, parse_format_arg,      "Format of output pixel data (iterm, kitty, sixels or symbols)", NULL },
        { "font-ratio",  '\0', 0, G_OPTION_ARG_CALLBACK, parse_font_ratio_arg,  "Font ratio", NULL },
        { "fuzz-options", '\0', 0, G_OPTION_ARG_NONE,    &options.fuzz_options, "Fuzz the options", NULL },
        { "glyph-file",  '\0', 0, G_OPTION_ARG_CALLBACK, parse_glyph_file_arg,  "Glyph file", NULL },
        { "grid",        '\0', 0, G_OPTION_ARG_CALLBACK, parse_grid_arg,        "Grid", NULL },
        { "grid-on",     'g',  0, G_OPTION_ARG_NONE,     &options.grid_on,      "Grid on", NULL },
        { "invert",      '\0', 0, G_OPTION_ARG_NONE,     &options.invert,       "Invert foreground/background", NULL },
        { "label",       '\0', 0, G_OPTION_ARG_CALLBACK, parse_label_arg,       "Print labels", NULL },
        { "label-on",    'l',  0, G_OPTION_ARG_NONE,     &options.label,        "Print labels on", NULL },
        { "link",        '\0', 0, G_OPTION_ARG_CALLBACK, parse_link_arg,        "Link labels", NULL },
        { "margin-bottom", '\0', 0, G_OPTION_ARG_INT,    &options.margin_bottom,  "Bottom margin", NULL },
        { "margin-right", '\0', 0, G_OPTION_ARG_INT,     &options.margin_right,  "Right margin", NULL },
        { "optimize",    'O',  0, G_OPTION_ARG_INT,      &options.optimization_level,  "Optimization", NULL },
        { "passthrough", '\0', 0, G_OPTION_ARG_CALLBACK, parse_passthrough_arg, "Passthrough", NULL },
        { "polite",      '\0', 0, G_OPTION_ARG_CALLBACK, parse_polite_arg,      "Polite", NULL },
        { "preprocess",  'p',  0, G_OPTION_ARG_CALLBACK, parse_preprocess_arg,  "Preprocessing", NULL },
        { "probe",       '\0', 0, G_OPTION_ARG_CALLBACK, parse_probe_arg,       "Terminal probing", NULL },
        { "relative",    '\0', 0, G_OPTION_ARG_CALLBACK, parse_relative_arg,    "Relative", NULL },
        { "work",        'w',  0, G_OPTION_ARG_INT,      &options.work_factor,  "Work factor", NULL },
        { "scale",       '\0', 0, G_OPTION_ARG_CALLBACK, parse_scale_arg,       "Scale", NULL },
        { "size",        's',  0, G_OPTION_ARG_CALLBACK, parse_size_arg,        "Output size", NULL },
        { "speed",       '\0', 0, G_OPTION_ARG_CALLBACK, parse_anim_speed_arg,  "Animation speed", NULL },
        { "stretch",     '\0', 0, G_OPTION_ARG_NONE,     &options.stretch,      "Stretch image to fix output dimensions", NULL },
        { "symbols",     '\0', 0, G_OPTION_ARG_CALLBACK, parse_symbols_arg,     "Output symbols", NULL },
        { "threads",     '\0', 0, G_OPTION_ARG_INT,      &options.n_threads,    "Number of threads", NULL },
        { "threshold",   't',  0, G_OPTION_ARG_CALLBACK, parse_threshold_arg,   "Transparency threshold", NULL },
        { "view-size",   '\0', 0, G_OPTION_ARG_CALLBACK, parse_view_size_arg,   "View size", NULL },
        { "watch",       '\0', 0, G_OPTION_ARG_NONE,     &options.watch,        "Watch a file's contents", NULL },
        /* Deprecated: Equivalent to --scale max */
        { "zoom",        '\0', 0, G_OPTION_ARG_NONE,     &options.zoom,         "Allow scaling up beyond one character per pixel", NULL },
        { 0 }
    };
    ChafaCanvasMode canvas_mode;
    ChafaPixelMode pixel_mode;
    ChafaPassthrough passthrough;
    gboolean polite;
    gchar **envp;

    envp = g_get_environ ();
    memset (&options, 0, sizeof (options));

    context = g_option_context_new ("[COMMAND] [OPTION...]");
    g_option_context_set_help_enabled (context, FALSE);
    g_option_context_add_main_entries (context, option_entries, NULL);

    options.executable_name = g_strdup ((*argv) [0]);

    /* Defaults */

    options.is_interactive = isatty (STDIN_FILENO) && isatty (STDOUT_FILENO);
    detect_terminal (envp, &options.term_info, &canvas_mode, &pixel_mode,
                     &passthrough, &options.symbol_map, &options.fill_symbol_map,
                     &polite);

    options.mode = CHAFA_CANVAS_MODE_MAX;  /* Unset */
    options.pixel_mode = pixel_mode;
    options.pixel_mode_set = FALSE;
    options.polite = polite;
    options.dither_mode = CHAFA_DITHER_MODE_NONE;
    options.dither_grain_width = -1;  /* Unset */
    options.dither_grain_height = -1;  /* Unset */
    options.dither_intensity = 1.0;
    options.animate = TRUE;
    options.horiz_align = CHAFA_ALIGN_MAX;  /* Unset */
    options.vert_align = CHAFA_ALIGN_MAX;  /* Unset */
    options.probe = CHICLE_TRISTATE_AUTO;
    options.probe_duration = CHICLE_PROBE_DURATION_DEFAULT;
    options.preprocess = TRUE;
    options.relative_set = FALSE;
    options.fg_only = FALSE;
    options.color_extractor = CHAFA_COLOR_EXTRACTOR_AVERAGE;
    options.color_space = CHAFA_COLOR_SPACE_RGB;
    options.view_width = -1;  /* Unset */
    options.view_height = -1;  /* Unset */
    options.width = -1;  /* Unset */
    options.height = -1;  /* Unset */
    options.grid_on = FALSE;
    options.grid_width = -1;  /* Unset */
    options.grid_height = -1;  /* Unset */
    options.fit_to_width = FALSE;
    options.font_ratio = -1.0;  /* Unset */
    options.margin_bottom = -1;  /* Unset */
    options.margin_right = -1;  /* Unset */
    options.scale = -1.0;  /* Unset */
    options.work_factor = 5;
    options.optimization_level = G_MININT;  /* Unset */
    options.n_threads = -1;
    options.fg_color = 0xffffff;
    options.bg_color = 0x000000;
    options.transparency_threshold = G_MAXDOUBLE;  /* Unset */
    options.file_duration_s = -1.0;  /* Unset */
    options.anim_fps = -1.0;
    options.anim_speed_multiplier = 1.0;
    options.use_exact_size = CHICLE_TRISTATE_AUTO;
    options.cell_width = 10;
    options.cell_height = 20;
    options.label = FALSE;
    options.link_labels = CHICLE_TRISTATE_AUTO;
    options.use_unicode = TRUE;

    if (!g_option_context_parse (context, argc, argv, &error))
    {
        g_printerr ("%s: %s\n", options.executable_name, error->message);
        goto out;
    }

    term = chafa_term_new (NULL,
                           global_path_queue_n_stdin == 0 ? STDIN_FILENO : -1,
                           STDOUT_FILENO,
                           STDERR_FILENO);

    /* Parser kludges */

    if (options.grid_on && options.grid_width == -1 && options.grid_height == -1)
    {
        options.grid_width = options.grid_height = CHICLE_GRID_AUTO;
    }

    /* If help, version info or config dump was requested, print it and bail out */

    if (options.show_help)
    {
        print_summary ();
        options.skip_processing = TRUE;
    }

    if (options.show_version)
    {
        print_version ();
        options.skip_processing = TRUE;
    }

    if (options.do_dump_detect)
    {
        dump_settings (options.term_info, canvas_mode, pixel_mode, passthrough,
                       polite);
        options.skip_processing = TRUE;
    }

    /* Some options preclude normal arg processing */

    if (options.skip_processing)
    {
        result = TRUE;
        goto out;
    }

    /* Optionally fuzz the options */

    if (options.fuzz_options && *argc > 1)
    {
        fuzz_options_with_file (&options, (*argv) [1]);
    }

    /* Synchronous probe for sixels, default colors, etc. */

    if ((options.probe == CHICLE_TRISTATE_TRUE
         || options.probe == CHICLE_TRISTATE_AUTO)
        && options.probe_duration >= 0.0)
    {
        chafa_term_sync_probe (term, options.probe_duration * 1000);

        if (!options.pixel_mode_set)
        {
            options.pixel_mode = chafa_term_info_get_best_pixel_mode (
                chafa_term_get_term_info (term));
        }

        if (!options.fg_color_set)
        {
            gint32 color = chafa_term_get_default_fg_color (term);
            if (color >= 0)
                options.fg_color = color;
        }

        if (!options.bg_color_set)
        {
            gint32 color = chafa_term_get_default_bg_color (term);
            if (color >= 0)
                options.bg_color = color;
        }
    }

    /* Detect terminal geometry */

    chafa_term_get_size_px (term,
                            &detected_term_size.width_pixels,
                            &detected_term_size.height_pixels);
    chafa_term_get_size_cells (term,
                               &detected_term_size.width_cells,
                               &detected_term_size.height_cells);

    /* Fall back on COLUMNS. It's the best we can do in some environments (e.g. CI). */

    if (detected_term_size.width_cells < 1)
    {
        const gchar *columns_str = g_getenv ("COLUMNS");
        if (columns_str)
            detected_term_size.width_cells = atoi (columns_str);
        if (detected_term_size.width_cells < 1)
            detected_term_size.width_cells = -1;
    }

    /* Figure out cell size if we can */

    if (detected_term_size.width_cells > 0
        && detected_term_size.height_cells > 0
        && detected_term_size.width_pixels > 0
        && detected_term_size.height_pixels > 0)
    {
        options.cell_width = detected_term_size.width_pixels / detected_term_size.width_cells;
        options.cell_height = detected_term_size.height_pixels / detected_term_size.height_cells;
    }

    /* Apply the font ratio if specified, or derive it if not */

    if (options.cell_width > 0 && options.cell_height > 0)
    {
        if (options.pixel_mode == CHAFA_PIXEL_MODE_SYMBOLS && options.font_ratio > 0.0)
        {
            options.cell_height = options.cell_width / options.font_ratio;
        }
        else
        {
            options.font_ratio = (gdouble) options.cell_width / (gdouble) options.cell_height;
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

    /* On some terminals, sixel output leaves the cursor below the image, so
     * force a bottom margin of at least one even if the user wanted less. */
    if (options.margin_bottom < 1
        && options.pixel_mode == CHAFA_PIXEL_MODE_SIXELS
        && (chafa_term_info_get_quirks (chafa_term_get_term_info (term))
            & CHAFA_TERM_QUIRK_SIXEL_OVERSHOOT))
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

    if (options.view_width < 0 && options.view_height < 0)
    {
        options.view_width = detected_term_size.width_cells;
        options.view_height = detected_term_size.height_cells;

        if (options.view_width < 0 && options.view_height < 0)
        {
            options.view_width = 80;
            options.view_height = 25;
        }
    }

    if (using_detected_size &&
        ((options.stretch && (options.view_width < 0 || options.view_height < 0))
         || (options.fit_to_width && options.view_width < 0)))
    {
        g_printerr ("%s: Refusing to stretch images to infinity.\n", options.executable_name);
        goto out;
    }

    if (options.view_width < 0)
        options.view_width = CHICLE_CELL_EXTENT_AUTO_MAX;
    if (options.view_height < 0)
        options.view_height = CHICLE_CELL_EXTENT_AUTO_MAX;

    if (using_detected_size)
    {
        options.width = options.view_width;
        options.height = options.view_height;

        options.width = (options.width > options.margin_right)
            ? options.width - options.margin_right : 1;

        options.height = (options.height > options.margin_bottom)
            ? options.height - options.margin_bottom : 1;

        if (options.fit_to_width)
        {
            options.height = CHICLE_CELL_EXTENT_AUTO_MAX;

            /* Fit-to-width only makes sense if we respect the aspect ratio.
             * We could complain about infinities here, but let's be nice and
             * let it override --stretch instead. */
            options.stretch = FALSE;

            /* --fit-width implies --scale max */
            options.scale = CHICLE_SCALE_MAX;
        }
    }

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
        if (options.scale <= 0.0)
            options.scale = 4.0;
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
        if (options.scale <= 0.0)
            options.scale = 1.0;
    }

    if (options.pixel_mode == CHAFA_PIXEL_MODE_SIXELS)
    {
        if (!options.dither_mode_set)
            options.dither_mode = CHAFA_DITHER_MODE_NOISE;
    }

    /* Apply detected passthrough if auto */
    if (!options.passthrough_set)
    {
        /* tmux 3.4+ supports sixels without the need for passthrough. We check
         * this here instead of in detect_terminal() because we need to react to
         * a user-requested sixel mode too */
        if (options.pixel_mode == CHAFA_PIXEL_MODE_SIXELS
            && passthrough == CHAFA_PASSTHROUGH_TMUX
            && get_tmux_version (envp) >= 3004)
        {
            options.passthrough = CHAFA_PASSTHROUGH_NONE;
        }
        else
        {
            options.passthrough = passthrough;
        }
    }

    /* Resolve grid dimensions if necessary. Items need to be a bit bigger
     * in symbols mode in order to be legible. */

    if (options.grid_width == CHICLE_GRID_AUTO || options.grid_height == CHICLE_GRID_AUTO)
    {
        gint item_width = (MIN (options.width,
                                options.pixel_mode == CHAFA_PIXEL_MODE_SYMBOLS ? 19 : 12));

        options.grid_width = options.width / item_width;
        options.grid_height = -1;
    }

    /* Set default alignment depending on run mode */

    if (options.horiz_align == CHAFA_ALIGN_MAX)
    {
        options.horiz_align = ((options.grid_width > 0 || options.grid_height > 0)
                               ? CHAFA_ALIGN_CENTER
                               : CHAFA_ALIGN_START);
    }
    if (options.vert_align == CHAFA_ALIGN_MAX)
    {
        options.vert_align = ((options.grid_width > 0 || options.grid_height > 0)
                              ? CHAFA_ALIGN_END
                              : CHAFA_ALIGN_START);
    }

    /* Optionally establish parking row for the cursor */

    options.have_parking_row = ((using_detected_size || options.vert_align == CHAFA_ALIGN_END)
                                && options.margin_bottom == 0) ? FALSE : TRUE;

    /* Apply tmux workarounds only if both detected and desired, and a
     * graphics protocol is desired */
    if (passthrough == CHAFA_PASSTHROUGH_TMUX
        && options.passthrough == CHAFA_PASSTHROUGH_TMUX
        && options.pixel_mode != CHAFA_PIXEL_MODE_SYMBOLS)
        chicle_apply_passthrough_workarounds_tmux ();

    if (options.work_factor < 1 || options.work_factor > 9)
    {
        g_printerr ("%s: Work factor must be in the range [1-9].\n", options.executable_name);
        goto out;
    }

    if (options.transparency_threshold == G_MAXDOUBLE)
        options.transparency_threshold = 0.5;
    else
        options.transparency_threshold_set = TRUE;

    if (options.transparency_threshold < 0.0 || options.transparency_threshold > 1.0)
    {
        g_printerr ("%s: Transparency threshold %.1lf is not in the range [0.0-1.0].\n", options.executable_name, options.transparency_threshold);
        goto out;
    }

    /* Resolve whether to hyperlink */

    if (options.link_labels == CHICLE_TRISTATE_AUTO)
        options.link_labels = isatty (STDOUT_FILENO) ? CHICLE_TRISTATE_TRUE : CHICLE_TRISTATE_FALSE;

    /* Collect filenames and validate count and correct usage of stdin */

    if (*argc > 1)
    {
        options.args = collect_variable_arguments (argc, argv, 1);
    }
    else if (global_path_queue_n_stdin < 1 && !isatty (STDIN_FILENO))
    {
        /* Receiving data through a pipe, and no file arguments. Act as if
         * invoked with "chafa -". */

        options.args = g_list_append (NULL, g_strdup ("-"));
    }
    else if (chicle_path_queue_get_length (global_path_queue) == 0)
    {
        /* No arguments, no pipe, no file lists, and no cry for help. */
        print_brief_summary ();
        goto out;
    }

    if (count_dash_strings (options.args) + global_path_queue_n_stdin > 1)
    {
        g_printerr ("%s: Dash '-' to pipe from standard input can be used at most once.\n",
                    options.executable_name);
        goto out;
    }

    if (options.watch)
    {
        if (g_list_length (options.args) != 1 || chicle_path_queue_get_length (global_path_queue) != 0)
        {
            g_printerr ("%s: Can only use --watch with exactly one file.\n", options.executable_name);
            goto out;
        }

        if (!strcmp (options.args->data, "-"))
        {
            g_printerr ("%s: Can only use --watch with a filename, not a pipe.\n", options.executable_name);
            goto out;
        }
    }

    if (options.zoom)
    {
        g_printerr ("%s: Warning: --zoom is deprecated, use --scale max instead.\n",
                    options.executable_name);
        options.scale = CHICLE_SCALE_MAX;
    }

    /* --stretch implies --scale max (same as --zoom) */
    if (options.stretch)
    {
        options.scale = CHICLE_SCALE_MAX;
    }

    /* `--exact-size on` overrides other sizing options */
    if (options.use_exact_size == CHICLE_TRISTATE_TRUE)
    {
        options.fit_to_width = FALSE;
        options.scale = 1.0;
        options.stretch = FALSE;
        using_detected_size = TRUE;
    }

    if (options.invert)
    {
        guint32 temp_color;

        temp_color = options.bg_color;
        options.bg_color = options.fg_color;
        options.fg_color = temp_color;
    }

    if (options.file_duration_s < 0.0
        && (!options.is_interactive
            || (options.args && options.args->next)
            || (global_n_path_streams > 0)))
    {
        /* Apply a zero default duration when we have multiple files or it looks
         * like we're part of a pipe; we don't want to get stuck if the user is
         * trying to e.g. batch convert files. Allow -d to override. */
        options.file_duration_s = CHICLE_FILE_DURATION_DEFAULT;
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
        goto out;
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

    result = TRUE;

out:
    g_option_context_free (context);
    g_strfreev (envp);

    return result;
}
