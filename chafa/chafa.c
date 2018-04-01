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

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#include <sys/ioctl.h>  /* ioctl */
#include <unistd.h>  /* STDOUT_FILENO */
#include <signal.h>  /* sigaction */
#include <termios.h>
#include <glib.h>
#include <wand/MagickWand.h>
#include "chafa/chafa.h"
#include "chafa/named-colors.h"

typedef struct
{
    gchar *executable_name;

    gboolean show_help;
    gboolean show_version;

    GList *args;
    ChafaCanvasMode mode;
    ChafaColorSpace color_space;
    ChafaSymbolMap *symbol_map;
    ChafaSymbolTags symbol_tags_specified;
    gboolean is_interactive;
    gboolean clear;
    gboolean verbose;
    gboolean invert;
    gboolean preprocess;
    gboolean preprocess_set;
    gboolean stretch;
    gint width, height;
    gdouble font_ratio;
    gint quality;
    guint32 fg_color;
    gboolean fg_color_set;
    guint32 bg_color;
    gboolean bg_color_set;
    gdouble transparency_threshold;
}
GlobalOptions;

static GlobalOptions options;
static gboolean interrupted_by_user;

static void
sigint_handler (G_GNUC_UNUSED int sig)
{
    interrupted_by_user = TRUE;
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
    "Copyright (C) 2018 Hans Petter Jansson\n"
    "This is free software; see the source for copying conditions.  There is NO\n"
    "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n";

static void
print_version (void)
{
    gchar *builtin_features_str = chafa_describe_features (chafa_get_builtin_features ());
    gchar *supported_features_str = chafa_describe_features (chafa_get_supported_features ());

    g_printerr ("Chafa version %s%s%s%s%s%s%s\n\n%s\n",
                CHAFA_VERSION,
                chafa_get_builtin_features () ? " -- built with [" : " -- no optional features",
                builtin_features_str,
                chafa_get_builtin_features () ? "]" : "",
                chafa_get_supported_features () ? " -- using [" : " -- none used",
                supported_features_str,
                chafa_get_supported_features () ? "]" : "",
                copyright_notice);

    g_free (builtin_features_str);
    g_free (supported_features_str);
}

static void
print_summary (void)
{
    const gchar *summary =
    "  Chafa (Character Art Facsimile) image-to-text converter.\n\n"

    "Options:\n"

    "  -h  --help         Show help.\n"
    "      --version      Show version.\n"
    "  -v, --verbose      Be verbose.\n\n"

    "      --bg COLOR     Background color of display (color name or hex).\n"
    "      --clear        Clear screen before processing each file.\n"
    "  -c, --colors       Set output color mode; one of [none, 2, 16, 240, 256,\n"
    "                     full]. Defaults to full (24-bit).\n"
    "      --color-space  Color space used for quantization; one of [rgb, din99d].\n"
    "                     Defaults to rgb, which is faster but less accurate.\n"
    "      --fg COLOR     Foreground color of display (color name or hex).\n"
    "      --font-ratio W/H  Target font's width/height ratio. Can be specified as\n"
    "                     a real number or a fraction. Defaults to 1/2.\n"
    "      --invert       Invert video. For display with bright backgrounds in\n"
    "                     color modes 2 and none. Swaps --fg and --bg.\n"
    "  -p, --preprocess   Image preprocessing [on, off]. Defaults to on with 16\n"
    "                     colors or lower, off otherwise.\n"
    "  -q, --quality Q    Desired quality [1-9]. 1 is the fastest, 9 is the most\n"
    "                     accurate. Defaults to 5.\n"
    "  -s, --size WxH     Set maximum output dimensions in columns and rows. By\n"
    "                     default this will be the size of your terminal, or 80x25 if\n"
    "                     size detection fails.\n"
    "      --stretch      Stretch image to fit output dimensions; ignore aspect.\n"
    "      --symbols ...  Specify character symbols to employ in final output.\n"
    "                     See below for full usage and a list of symbol classes.\n"
    "  -t  --transparency-threshold THRESHOLD  Threshold above which full\n"
    "                     transparency will be used [0.0 - 1.0].\n\n"

    "  Accepted classes for --symbols are [all, none, space, solid, stipple, block,\n"
    "  border, diagonal, dot, quad, half, hhalf, vhalf, inverted]. Some symbols\n"
    "  belong to multiple classes, e.g. diagonals are also borders. You can specify\n"
    "  a list of classes separated by commas, or prefix them with + and - to add\n"
    "  or remove symbols relative to the existing set. The ordering is significant.\n\n"

    "  The default symbol set is all-stipple-inverted for all modes except for\n"
    "  \"none\", which looks best when allowed to use inverted characters.\n\n"

    "Examples:\n"

    "  chafa -c 16 --color-space din99d --symbols -dot-stipple in.jpg\n"
    "    # Generate 16-color output with perceptual color picking and avoid using\n"
    "    # dot and stipple symbols.\n\n"

    "  chafa -c none --symbols block+border-solid in.png\n"
    "    # Generate uncolored output using block and border symbols, but avoid the\n"
    "    # solid block symbol.\n";

    g_printerr ("Usage:\n  %s [OPTION...] [FILE...]\n\n%s\n",
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
    else if (!g_ascii_strcasecmp (value, "240"))
        options.mode = CHAFA_CANVAS_MODE_INDEXED_240;
    else if (!g_ascii_strcasecmp (value, "256"))
        options.mode = CHAFA_CANVAS_MODE_INDEXED_256;
    else if (!g_ascii_strcasecmp (value, "full") || !g_ascii_strcasecmp (value, "rgb"))
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

typedef struct
{
    const gchar *name;
    ChafaSymbolTags sc;
}
SymMapping;

static gboolean
parse_symbol_tag (const gchar *name, gint len, ChafaSymbolTags *sc_out, GError **error)
{
    const SymMapping map [] =
    {
        { "all", CHAFA_SYMBOL_TAG_ALL },
        { "none", CHAFA_SYMBOL_TAG_NONE },
        { "space", CHAFA_SYMBOL_TAG_SPACE },
        { "solid", CHAFA_SYMBOL_TAG_SOLID },
        { "stipple", CHAFA_SYMBOL_TAG_STIPPLE },
        { "block", CHAFA_SYMBOL_TAG_BLOCK },
        { "border", CHAFA_SYMBOL_TAG_BORDER },
        { "diagonal", CHAFA_SYMBOL_TAG_DIAGONAL },
        { "dot", CHAFA_SYMBOL_TAG_DOT },
        { "quad", CHAFA_SYMBOL_TAG_QUAD },
        { "half", CHAFA_SYMBOL_TAG_HALF },
        { "hhalf", CHAFA_SYMBOL_TAG_HHALF },
        { "vhalf", CHAFA_SYMBOL_TAG_VHALF },
        { "inverted", CHAFA_SYMBOL_TAG_INVERTED },
        { NULL, 0 }
    };
    gint i;

    for (i = 0; map [i].name; i++)
    {
        if (!g_ascii_strncasecmp (map [i].name, name, len))
        {
            *sc_out = map [i].sc;
            return TRUE;
        }
    }

    g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                 "Unrecognized symbol class '%.*s'.",
                 len, name);
    return FALSE;
}

static gboolean
parse_symbols_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    const gchar *p0 = value;
    gboolean is_add = FALSE, is_remove = FALSE;
    gboolean result = TRUE;

    while (*p0)
    {
        ChafaSymbolTags sc;
        gint n;

        p0 += strspn (p0, " ");
        if (!*p0)
            break;

        p0 += strspn (p0, ",");
        if (*p0 == '-')
        {
            is_add = FALSE;
            is_remove = TRUE;
            p0++;
        }
        else if (*p0 == '+')
        {
            is_add = TRUE;
            is_remove = FALSE;
            p0++;
        }

        n = strspn (p0, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
        if (!n)
        {
            g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                         "Syntax error in symbol class list.");
            result = FALSE;
            break;
        }

        if (!parse_symbol_tag (p0, n, &sc, error))
        {
            result = FALSE;
            break;
        }

        p0 += n;

        if (is_add)
        {
            chafa_symbol_map_add_by_tags (options.symbol_map, sc);
        }
        else if (is_remove)
        {
            chafa_symbol_map_remove_by_tags (options.symbol_map, sc);
        }
        else
        {
            chafa_symbol_map_remove_by_tags (options.symbol_map, CHAFA_SYMBOL_TAG_ALL);
            chafa_symbol_map_add_by_tags (options.symbol_map, sc);
            is_add = TRUE;

            /* This effectively overrides all tags */
            sc = CHAFA_SYMBOL_TAG_ALL;
        }

        options.symbol_tags_specified |= sc;
    }

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
parse_preprocess_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, GError **error)
{
    gboolean result = TRUE;

    if (!g_ascii_strcasecmp (value, "on")
        || !g_ascii_strcasecmp (value, "yes"))
    {
        options.preprocess = TRUE;
    }
    else if (!g_ascii_strcasecmp (value, "off")
             || !g_ascii_strcasecmp (value, "no"))
    {
        options.preprocess = FALSE;
    }
    else
    {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Preprocessing must be one of [on, off].");
        result = FALSE;
    }

    options.preprocess_set = TRUE;
    return result;
}

static gboolean
parse_color_str (const gchar *value, guint32 *col_out, const gchar *error_message, GError **error)
{
    const NamedColor *named_color;
    guint32 col = 0x000000;
    gboolean result = TRUE;

    /* TODO: It would be nice to support rgb.txt here */

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
    else
    {
        col = col;
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
get_tty_size (void)
{
    struct winsize w;

    if (ioctl (STDOUT_FILENO, TIOCGWINSZ, &w) < 0)
        return;

    if (w.ws_col > 0)
        options.width = w.ws_col;

    /* We subtract one line for the user's prompt */
    if (w.ws_row > 2)
        options.height = w.ws_row - 1;

    /* If .ws_xpixel and .ws_ypixel were filled out, we could in theory
     * calculate aspect information for the font used. Unfortunately,
     * I have yet to find an environment where these fields get set to
     * anything other than zero. */
}

static struct termios saved_termios;

static void
tty_options_init (void)
{
    struct termios t;

    if (!options.is_interactive)
        return;

    tcgetattr (STDIN_FILENO, &saved_termios);
    t = saved_termios;
    t.c_lflag &= ~ECHO;
    tcsetattr (STDIN_FILENO, TCSANOW, &t);
}

static void
tty_options_deinit (void)
{
    if (!options.is_interactive)
        return;

    tcsetattr (STDIN_FILENO, TCSANOW, &saved_termios);
}

/* I really would've preferred to use termcap, but termcap contents
 * and the TERM env var are often unreliable/unrepresentative, so
 * instead we have this.
 *
 * If you're getting poor results, I'd love to hear about it so it can
 * be improved. */
static ChafaCanvasMode
detect_canvas_mode (void)
{
    const gchar *term;
    const gchar *colorterm;
    const gchar *vte_version;
    const gchar *tmux;
    ChafaCanvasMode mode = CHAFA_CANVAS_MODE_INDEXED_240;

    term = g_getenv ("TERM");
    if (!term) term = "";

    colorterm = g_getenv ("COLORTERM");
    if (!colorterm) colorterm = "";

    vte_version = g_getenv ("VTE_VERSION");
    if (!vte_version) vte_version = "";

    tmux = g_getenv ("TMUX");
    if (!tmux) tmux = "";

    /* Some terminals set COLORTERM=truecolor. However, this env var can
     * make its way into environments where truecolor is not desired
     * (e.g. screen sessions), so check it early on and override it later. */
    if (!g_ascii_strcasecmp (colorterm, "truecolor")
        || !g_ascii_strcasecmp (colorterm, "gnome-terminal")
        || !g_ascii_strcasecmp (colorterm, "xfce-terminal"))
        mode = CHAFA_CANVAS_MODE_TRUECOLOR;

    /* In a modern VTE we can rely on VTE_VERSION. It's a great terminal emulator
     * which supports truecolor. */
    if (strlen (vte_version) > 0)
        mode = CHAFA_CANVAS_MODE_TRUECOLOR;

    /* Terminals that advertise 256 colors usually support truecolor too,
     * (VTE, xterm) although some (xterm) may quantize to an indexed palette
     * regardless. */
    if (!strcmp (term, "xterm-256color"))
        mode = CHAFA_CANVAS_MODE_TRUECOLOR;

    /* 'screen' does not like truecolor at all, but 256 colors works fine.
     * Sometimes we'll see the outer terminal appended to the TERM string,
     * like so: screen.xterm-256color */
    if (!strncmp (term, "screen", 6))
    {
        mode = CHAFA_CANVAS_MODE_INDEXED_240;

        /* 'tmux' also sets TERM=screen, but it supports truecolor codes.
         * You may have to add the following to .tmux.conf to prevent
         * remapping to 256 colors:
         *
         * tmux set-option -ga terminal-overrides ",screen-256color:Tc" */
        if (strlen (tmux) > 0)
            mode = CHAFA_CANVAS_MODE_TRUECOLOR;
    }

    /* If TERM is "linux", we're probably on the Linux console, which supports
     * 16 colors only. It also sets COLORTERM=1.
     *
     * https://github.com/torvalds/linux/commit/cec5b2a97a11ade56a701e83044d0a2a984c67b4
     *
     * In theory we could emit truecolor codes and let the console remap,
     * but we get better results if we do the conversion ourselves, since we
     * can apply preprocessing and exotic color spaces. */
    if (!strcmp (term, "linux"))
        mode = CHAFA_CANVAS_MODE_INDEXED_16;

    return mode;
}

static gboolean
parse_options (int *argc, char **argv [])
{
    GError *error = NULL;
    GOptionContext *context;
    gboolean result = TRUE;
    const GOptionEntry option_entries [] =
    {
        { "help",        'h',  0, G_OPTION_ARG_NONE,     &options.show_help,    "Show help", NULL },
        { "version",     '\0', 0, G_OPTION_ARG_NONE,     &options.show_version, "Show version", NULL },
        { "verbose",     'v',  0, G_OPTION_ARG_NONE,     &options.verbose,      "Be verbose", NULL },
        { "bg",          '\0', 0, G_OPTION_ARG_CALLBACK, parse_bg_color_arg,    "Background color of display", NULL },
        { "clear",       '\0', 0, G_OPTION_ARG_NONE,     &options.clear,        "Clear", NULL },
        { "colors",      'c',  0, G_OPTION_ARG_CALLBACK, parse_colors_arg,      "Colors (none, 2, 16, 256, 240 or full)", NULL },
        { "color-space", '\0', 0, G_OPTION_ARG_CALLBACK, parse_color_space_arg, "Color space (rgb or din99d)", NULL },
        { "fg",          '\0', 0, G_OPTION_ARG_CALLBACK, parse_fg_color_arg,    "Foreground color of display", NULL },
        { "font-ratio",  '\0', 0, G_OPTION_ARG_CALLBACK, parse_font_ratio_arg,  "Font ratio", NULL },
        { "invert",      '\0', 0, G_OPTION_ARG_NONE,     &options.invert,       "Invert foreground/background", NULL },
        { "preprocess",  'p',  0, G_OPTION_ARG_CALLBACK, parse_preprocess_arg,  "Preprocessing", NULL },
        { "quality",     'q',  0, G_OPTION_ARG_INT,      &options.quality,      "Quality", NULL },
        { "size",        's',  0, G_OPTION_ARG_CALLBACK, parse_size_arg,        "Output size", NULL },
        { "stretch",     '\0', 0, G_OPTION_ARG_NONE,     &options.stretch,      "Stretch image to fix output dimensions", NULL },
        { "symbols",     '\0', 0, G_OPTION_ARG_CALLBACK, parse_symbols_arg,     "Output symbols", NULL },
        { "transparency-threshold", 't', 0, G_OPTION_ARG_DOUBLE, &options.transparency_threshold, "Transparency threshold", NULL },
        { NULL }
    };

    memset (&options, 0, sizeof (options));

    context = g_option_context_new ("[COMMAND] [OPTION...]");
    g_option_context_set_help_enabled (context, FALSE);
    g_option_context_add_main_entries (context, option_entries, NULL);

    options.executable_name = g_strdup ((*argv) [0]);

    /* Defaults */
    options.symbol_map = chafa_symbol_map_new ();
    chafa_symbol_map_add_by_tags (options.symbol_map, CHAFA_SYMBOL_TAG_ALL);
    chafa_symbol_map_remove_by_tags (options.symbol_map, CHAFA_SYMBOL_TAG_STIPPLE);

    options.is_interactive = isatty (STDIN_FILENO) && isatty (STDOUT_FILENO);
    options.mode = detect_canvas_mode ();
    options.color_space = CHAFA_COLOR_SPACE_RGB;
    options.width = 80;
    options.height = 25;
    options.font_ratio = 1.0 / 2.0;
    options.quality = 5;
    options.fg_color = 0xffffff;
    options.bg_color = 0x000000;
    options.transparency_threshold = -1.0;
    get_tty_size ();

    if (!g_option_context_parse (context, argc, argv, &error))
    {
        g_printerr ("Error: %s\n", error->message);
        return FALSE;
    }

    if (options.quality < 1 || options.quality > 9)
    {
        g_printerr ("Error: Quality must be in the range [1-9].\n");
        return FALSE;
    }

    if (options.show_version)
    {
        print_version ();
        return FALSE;
    }

    if (*argc < 2)
    {
        print_summary ();
        return FALSE;
    }

    options.args = collect_variable_arguments (argc, argv, 1);

    if (options.show_help)
    {
        print_summary ();
        return FALSE;
    }

    if (options.invert)
    {
        guint32 temp_color;

        temp_color = options.bg_color;
        options.bg_color = options.fg_color;
        options.fg_color = temp_color;
    }

    if (!options.preprocess_set)
    {
        options.preprocess = FALSE;

        if (options.mode == CHAFA_CANVAS_MODE_FGBG
            || options.mode == CHAFA_CANVAS_MODE_FGBG_BGFG
            || options.mode == CHAFA_CANVAS_MODE_INDEXED_16)
        {
            options.preprocess = TRUE;
        }
    }

    /* Since FGBG mode can't use escape sequences to invert, it really
     * needs inverted symbols. In other modes they will only slow us down,
     * so disable them unless the user specified otherwise. */
    if (options.mode != CHAFA_CANVAS_MODE_FGBG && !(options.symbol_tags_specified & CHAFA_SYMBOL_TAG_INVERTED))
        chafa_symbol_map_remove_by_tags (options.symbol_map, CHAFA_SYMBOL_TAG_INVERTED);

    g_option_context_free (context);

    return result;
}

static void
auto_orient_image (MagickWand *image)
{
#ifdef HAVE_MAGICK_AUTO_ORIENT_IMAGE
    MagickAutoOrientImage (image);
#else
    PixelWand *pwand = 0;

    switch (MagickGetImageOrientation (image))
    {
        case UndefinedOrientation:
        case TopLeftOrientation:
        default:
            break;
        case TopRightOrientation:
            MagickFlopImage (image);
            break;
        case BottomRightOrientation:
            pwand = NewPixelWand ();
            MagickRotateImage (image, pwand, 180.0);
        case BottomLeftOrientation:
            MagickFlipImage (image);
            break;
        case LeftTopOrientation:
            MagickTransposeImage (image);
            break;
        case RightTopOrientation:
            pwand = NewPixelWand ();
            MagickRotateImage (image, pwand, 90.0);
            break;
        case RightBottomOrientation:
            MagickTransverseImage (image);
            break;
        case LeftBottomOrientation:
            pwand = NewPixelWand ();
            MagickRotateImage (image, pwand, 270.0);
            break;
    }

    if (pwand)
        DestroyPixelWand (pwand);

    MagickSetImageOrientation (image, TopLeftOrientation);
#endif
}

static GString *
textify (guint8 *pixels,
         gint src_width, gint src_height,
         gint dest_width, gint dest_height)
{
    ChafaSymbolMap *symbol_map;
    ChafaCanvasConfig *config;
    ChafaCanvas *canvas;
    GString *gs;

    symbol_map = chafa_symbol_map_new ();
    config = chafa_canvas_config_new ();

    chafa_canvas_config_set_size (config, dest_width, dest_height);
    chafa_canvas_config_set_canvas_mode (config, options.mode);
    chafa_canvas_config_set_color_space (config, options.color_space);
    chafa_canvas_config_set_fg_color (config, options.fg_color);
    chafa_canvas_config_set_bg_color (config, options.bg_color);
    if (options.transparency_threshold >= 0.0)
        chafa_canvas_config_set_transparency_threshold (config, options.transparency_threshold);

    chafa_canvas_config_set_symbol_map (config, options.symbol_map);

    /* Quality switch takes values [1..9], we normalize to [0.0..1.0] to
     * get the work factor. */
    chafa_canvas_config_set_work_factor (config, (options.quality - 1) / 8.0);

    canvas = chafa_canvas_new (config);
    chafa_canvas_set_contents_rgba (canvas, pixels, src_width, src_height, src_width * 4);
    gs = chafa_canvas_build_ansi (canvas);

    chafa_canvas_unref (canvas);
    chafa_canvas_config_unref (config);
    chafa_symbol_map_unref (symbol_map);
    return gs;
}

static void
process_image (MagickWand *wand, gint *dest_width_out, gint *dest_height_out)
{
    gint src_width, src_height;
    gint dest_width, dest_height;

    auto_orient_image (wand);

    src_width = MagickGetImageWidth (wand);
    src_height = MagickGetImageHeight (wand);

    dest_width = options.width;
    dest_height = options.height;

    if (!options.stretch || dest_width < 0 || dest_height < 0)
    {
        gdouble src_aspect;
        gdouble dest_aspect;

        src_aspect = src_width / (gdouble) src_height;
        dest_aspect = (dest_width / (gdouble) dest_height) * options.font_ratio;

        if (dest_width < 1)
        {
            dest_width = dest_height * (src_aspect / options.font_ratio) + 0.5;
        }
        else if (dest_height < 1)
        {
            dest_height = (dest_width / src_aspect) * options.font_ratio + 0.5;
        }
        else if (src_aspect > dest_aspect)
        {
            dest_height = dest_width * (options.font_ratio / src_aspect);
        }
        else
        {
            dest_width = dest_height * (src_aspect / options.font_ratio);
        }

        dest_width = MAX (dest_width, 1);
        dest_height = MAX (dest_height, 1);
    }

    if (options.quality >= 4 || options.preprocess)
    {
        gint new_width = CHAFA_SYMBOL_WIDTH_PIXELS * dest_width;
        gint new_height = CHAFA_SYMBOL_HEIGHT_PIXELS * dest_height;

        if (new_width < src_width || new_height < src_height)
        {
            src_width = new_width;
            src_height = new_height;

#if defined(HAVE_MAGICK_RESIZE_IMAGE_4)
            MagickResizeImage (wand, src_width, src_height, LanczosFilter);
#elif defined(HAVE_MAGICK_RESIZE_IMAGE_5)
            MagickResizeImage (wand, src_width, src_height, LanczosFilter, 1.0);
#else
# warning No valid MagickResizeImage detected. Trying four arguments.
            MagickResizeImage (wand, src_width, src_height, LanczosFilter);
#endif
        }
    }

    if (options.preprocess && !interrupted_by_user)
    {
        MagickModulateImage (wand, 100, 150, 100);
        MagickBrightnessContrastImage (wand, 0, 40);
    }

    if (dest_width_out)
        *dest_width_out = dest_width;
    if (dest_height_out)
        *dest_height_out = dest_height;
}

typedef struct
{
    GString *gs;
    gint dest_width, dest_height;
    gint delay_ms;
}
GroupFrame;

typedef struct
{
    GList *frames;
}
Group;

static void
group_build (Group *group, MagickWand *wand)
{
    memset (group, 0, sizeof (*group));

    for (MagickResetIterator (wand); !interrupted_by_user; )
    {
        GroupFrame *frame;

        if (!MagickNextImage (wand))
            break;

        frame = g_new0 (GroupFrame, 1);

        process_image (wand, &frame->dest_width, &frame->dest_height);
        frame->delay_ms = MagickGetImageDelay (wand) * 10;
        if (frame->delay_ms == 0)
            frame->delay_ms = 50;

        /* String representation is built on demand and cached */

        group->frames = g_list_prepend (group->frames, frame);
    }

    group->frames = g_list_reverse (group->frames);
}

static void
group_clear (Group *group)
{
    GList *l;

    for (l = group->frames; l; l = g_list_next (l))
    {
        GroupFrame *frame = l->data;

        if (frame->gs)
            g_string_free (frame->gs, TRUE);
        g_free (frame);
    }

    g_list_free (group->frames);
    memset (group, 0, sizeof (*group));
}

static void
run (const gchar *filename, gboolean is_single_file)
{
    MagickWand *wand = NULL;
    guint8 *pixels;
    gboolean is_first_frame = TRUE;
    gboolean is_animation;
    GTimer *timer;
    Group group = { NULL };
    GList *l;

    timer = g_timer_new ();

    wand = NewMagickWand();
    if (MagickReadImage (wand, filename) < 1)
    {
        gchar *error_str;
        ExceptionType severity;

        error_str = MagickGetException (wand, &severity);
        g_printerr ("Error: %s\n", error_str);
        MagickRelinquishMemory (error_str);
        goto out;
    }

    if (interrupted_by_user)
        goto out;

    is_animation = MagickGetNumberImages (wand) > 1 ? TRUE : FALSE;

    if (is_animation)
    {
        MagickWand *wand2 = MagickCoalesceImages (wand);
        wand = DestroyMagickWand (wand);
        wand = wand2;
    }

    if (interrupted_by_user)
        goto out;

    group_build (&group, wand);

    if (interrupted_by_user)
        goto out;

    do
    {
        /* Outer loop repeats animation if desired */

        MagickResetIterator (wand);

        for (l = group.frames; l && !interrupted_by_user; l = g_list_next (l))
        {
            GroupFrame *frame = l->data;
            gint elapsed_ms, remain_ms;

            MagickNextImage (wand);

            g_timer_start (timer);

            if (!frame->gs)
            {
                gint src_width, src_height;

                src_width = MagickGetImageWidth (wand);
                src_height = MagickGetImageHeight (wand);

                pixels = g_malloc (src_width * src_height * 4);
                MagickExportImagePixels (wand,
                                         0, 0,
                                         src_width, src_height,
                                         "RGBA",
                                         CharPixel,
                                         pixels);

                frame->gs = textify (pixels,
                                     src_width, src_height,
                                     frame->dest_width, frame->dest_height);
                g_free (pixels);
            }

            if (options.clear)
            {
                if (is_first_frame)
                {
                    printf ("\x1b[2J");
                }

                /* Home cursor between frames */
                printf ("\x1b[0f");
            }
            else if (!is_first_frame)
            {
                /* Cursor up N steps */
                printf ("\x1b[%dA", frame->dest_height);
            }

            fwrite (frame->gs->str, sizeof (gchar), frame->gs->len, stdout);
            fflush (stdout);

            if (is_animation)
            {
                /* Account for time spent converting and printing frame */
                elapsed_ms = g_timer_elapsed (timer, NULL) * 1000.0;
                remain_ms = frame->delay_ms - elapsed_ms;
                remain_ms = MAX (remain_ms, 0);
                g_usleep (remain_ms * 1000);
            }

            is_first_frame = FALSE;
        }
    }
    while (options.is_interactive && is_animation && is_single_file && !interrupted_by_user);

out:
    DestroyMagickWand (wand);
    group_clear (&group);
    g_timer_destroy (timer);
}

static int
run_all (GList *filenames)
{
    GList *l;
    gboolean is_single_file = FALSE;

    if (!filenames)
        return 0;

    if (!filenames->next)
        is_single_file = TRUE;

    tty_options_init ();
    MagickWandGenesis ();

    for (l = filenames; l && !interrupted_by_user; l = g_list_next (l))
    {
        gchar *filename = l->data;
        run (filename, is_single_file);
    }

    MagickWandTerminus ();
    tty_options_deinit ();
    return 0;
}

static void
proc_init (void)
{
    struct sigaction sa = { 0 };

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

    ret = run_all (options.args);

    if (options.symbol_map)
        chafa_symbol_map_unref (options.symbol_map);
    return ret;
}
