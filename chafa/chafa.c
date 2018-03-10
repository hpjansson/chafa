#include "config.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#include <sys/ioctl.h>  /* ioctl */
#include <unistd.h>  /* STDOUT_FILENO */
#include <glib.h>
#include <wand/MagickWand.h>
#include "chafa/chafa.h"

typedef struct
{
    gchar *executable_name;

    gboolean show_help;
    gboolean show_version;

    GList *args;
    ChafaCanvasMode mode;
    ChafaColorSpace color_space;
    ChafaSymbolClass inc_sym;
    ChafaSymbolClass exc_sym;
    gboolean verbose;
    gboolean invert;
    gboolean preprocess;
    gboolean preprocess_set;
    gboolean stretch;
    gint width, height;
    gdouble font_ratio;
    gint quality;
    guint32 transparency_color;
    gboolean transparency_color_set;
    gdouble transparency_threshold;
}
GlobalOptions;

static GlobalOptions options;

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
    if (len != strlen (p0) || len < 6)
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

static void
print_version (void)
{
    g_printerr ("Chafa version %s\n", VERSION);
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

    "  -c, --colors       Set output color mode; one of [none, 2, 16, 240, 256,\n"
    "                     full]. Defaults to full (24-bit).\n"
    "      --color-space  Color space used for quantization; one of [rgb, din99d].\n"
    "                     Defaults to rgb, which is faster but less accurate.\n"
    "      --font-ratio W/H  Target font's width/height ratio. Can be specified as\n"
    "                     a real number or a fraction. Defaults to 1/2.\n"
    "  -i, --invert       Invert video. For display with bright backgrounds in\n"
    "                     color modes 2 and none.\n"
    "  -p, --preprocess   Image preprocessing [on, off]. Defaults to on with 16\n"
    "                     colors or lower, off otherwise.\n"
    "  -q, --quality Q    Desired quality [1-9]. 1 is the fastest, 9 is the most\n"
    "                     accurate. Defaults to 5.\n"
    "  -s, --size WxH     Set maximum output dimensions in columns and rows. By\n"
    "                     default this will be the size of your terminal, or 80x25 if\n"
    "                     size detection fails.\n"
    "      --stretch      Stretch image to fit output dimensions; ignore aspect.\n"
    "      --symbols ...  Set output symbols to use based on an inclusion and an\n"
    "                     exclusion list. Each symbol identifier is preceded by an\n"
    "                     optional + to include, or - to exclude.\n"
    "  -t, --transparency-color COLOR  Color to substitute for transparency [black,\n"
    "                     white].\n"
    "      --transparency-threshold THRESHOLD  Threshold above which full\n"
    "                     transparency will be used [0.0 - 1.0].\n\n"

    "  Accepted classes for --symbols are [all, none, space, solid, stipple, block,\n"
    "  border, diagonal, dot]. Use \"none\" to clear the include/exclude lists.\n"
    "  Some symbols belong to multiple classes, e.g. diagonals are also borders.\n\n"

    "Examples:\n"

    "  chafa -c 16 --color-space din99d --symbols -dot-stipple in.jpg\n"
    "    # Generate 16-color output with perceptual color picking and avoid using\n"
    "    # dot and stipple symbols.\n\n"

    "  chafa -c none --symbols none+block+border-solid in.png\n"
    "    # Generate uncolored output using block and border symbols, but avoid the\n"
    "    # solid block symbol.\n";

    g_printerr ("Usage:\n  %s [OPTION...] [FILE...]\n\n%s\n",
                options.executable_name,
                summary);
}

static gboolean
parse_colors_arg (const gchar *option_name, const gchar *value, gpointer data, GError **error)
{
    gboolean result = TRUE;

    if (!strcasecmp (value, "none"))
	options.mode = CHAFA_CANVAS_MODE_SHAPES_WHITE_ON_BLACK;
    else if (!strcasecmp (value, "2"))
	options.mode = CHAFA_CANVAS_MODE_INDEXED_WHITE_ON_BLACK;
    else if (!strcasecmp (value, "16"))
	options.mode = CHAFA_CANVAS_MODE_INDEXED_16;
    else if (!strcasecmp (value, "240"))
	options.mode = CHAFA_CANVAS_MODE_INDEXED_240;
    else if (!strcasecmp (value, "256"))
	options.mode = CHAFA_CANVAS_MODE_INDEXED_256;
    else if (!strcasecmp (value, "full") || !strcasecmp (value, "rgb"))
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
parse_color_space_arg (const gchar *option_name, const gchar *value, gpointer data, GError **error)
{
    gboolean result = TRUE;

    if (!strcasecmp (value, "rgb"))
        options.color_space = CHAFA_COLOR_SPACE_RGB;
    else if (!strcasecmp (value, "din99d"))
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
parse_font_ratio_arg (const gchar *option_name, const gchar *value, gpointer data, GError **error)
{
    gboolean result = TRUE;
    gdouble ratio = -1.0;
    gint width = -1, height = -1;
    gint o = 0;

    sscanf (value, "%u/%u%n", &width, &height, &o);
    if (width < 0 || height < 0)
        sscanf (value, "%u:%u%n", &width, &height, &o);
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
    ChafaSymbolClass sc;
}
SymMapping;

static gboolean
parse_symbol_class (const gchar *name, gint len, ChafaSymbolClass *sc_out, GError **error)
{
    const SymMapping map [] =
    {
        { "all", CHAFA_SYMBOLS_ALL },
        { "none", CHAFA_SYMBOLS_NONE },
        { "space", CHAFA_SYMBOL_CLASS_SPACE },
        { "solid", CHAFA_SYMBOL_CLASS_SOLID },
        { "stipple", CHAFA_SYMBOL_CLASS_STIPPLE },
        { "block", CHAFA_SYMBOL_CLASS_BLOCK },
        { "border", CHAFA_SYMBOL_CLASS_BORDER },
        { "diagonal", CHAFA_SYMBOL_CLASS_DIAGONAL },
        { "dot", CHAFA_SYMBOL_CLASS_DOT },
        { "quad", CHAFA_SYMBOL_CLASS_QUAD },
        { "half", CHAFA_SYMBOL_CLASS_HALF },
        { NULL, 0 }
    };
    gint i;

    for (i = 0; map [i].name; i++)
    {
        if (!strncasecmp (map [i].name, name, len))
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
parse_symbols_arg (const gchar *option_name, const gchar *value, gpointer data, GError **error)
{
    const gchar *p0 = value;
    gboolean result = TRUE;

    while (*p0)
    {
        ChafaSymbolClass sc;
        gboolean is_exc = FALSE;
        gint n;

        p0 += strspn (p0, " ");
        if (!*p0)
            break;

        p0 += strspn (p0, ",");
        if (*p0 == '-')
        {
            is_exc = TRUE;
            p0++;
        }
        else if (*p0 == '+')
        {
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

        if (!parse_symbol_class (p0, n, &sc, error))
        {
            result = FALSE;
            break;
        }

        p0 += n;

        if (is_exc)
        {
            if (sc == CHAFA_SYMBOLS_NONE)
                options.exc_sym = CHAFA_SYMBOLS_NONE;
            options.exc_sym |= sc;
        }
        else
        {
            if (sc == CHAFA_SYMBOLS_NONE)
                options.inc_sym = CHAFA_SYMBOLS_NONE;
            options.inc_sym |= sc;
        }
    }

    return result;
}

static gboolean
parse_size_arg (const gchar *option_name, const gchar *value, gpointer data, GError **error)
{
    gboolean result = TRUE;
    gint width = -1, height = -1;
    gint n;
    gint o = 0;

    n = sscanf (value, "%u%n", &width, &o);

    if (value [o] == 'x' && value [o + 1] != '\0')
    {
        gint o2;

        n = sscanf (value + o + 1, "%u%n", &height, &o2);
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
parse_preprocess_arg (const gchar *option_name, const gchar *value, gpointer data, GError **error)
{
    gboolean result = TRUE;

    if (!strcasecmp (value, "on")
	|| !strcasecmp (value, "yes"))
    {
        options.preprocess = TRUE;
    }
    else if (!strcasecmp (value, "off")
	     || !strcasecmp (value, "no"))
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
parse_transparency_color_arg (const gchar *option_name, const gchar *value, gpointer data, GError **error)
{
    gboolean result = TRUE;
    guint32 col;

    if (!strcasecmp (value, "white"))
    {
        options.transparency_color = 0xffffff;
    }
    else if (!strcasecmp (value, "black"))
    {
        options.transparency_color = 0x000000;
    }
    else if (!parse_color (value, &col, NULL))
    {
	g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		     "Unrecognized transparency color '%s'.", value);
	result = FALSE;
    }
    else
    {
        options.transparency_color = col;
    }

    options.transparency_color_set = TRUE;
    return result;
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
    if (!strcasecmp (colorterm, "truecolor")
        || !strcasecmp (colorterm, "gnome-terminal")
        || !strcasecmp (colorterm, "xfce-terminal"))
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
        { "colors",      'c',  0, G_OPTION_ARG_CALLBACK, parse_colors_arg,      "Colors (none, 2, 16, 256, 240 or full)", NULL },
        { "color-space", '\0', 0, G_OPTION_ARG_CALLBACK, parse_color_space_arg, "Color space (rgb or din99d)", NULL },
        { "font-ratio",  '\0', 0, G_OPTION_ARG_CALLBACK, parse_font_ratio_arg,  "Font ratio", NULL },
        { "invert",      'i',  0, G_OPTION_ARG_NONE,     &options.invert,       "Inverse video (for black on white terminals)", NULL },
        { "preprocess",  'p',  0, G_OPTION_ARG_CALLBACK, parse_preprocess_arg,  "Preprocessing", NULL },
        { "quality",     'q',  0, G_OPTION_ARG_INT,      &options.quality,      "Quality", NULL },
        { "size",        's',  0, G_OPTION_ARG_CALLBACK, parse_size_arg,        "Output size", NULL },
        { "stretch",     '\0', 0, G_OPTION_ARG_NONE,     &options.stretch,      "Stretch image to fix output dimensions", NULL },
        { "symbols",     '\0', 0, G_OPTION_ARG_CALLBACK, parse_symbols_arg,     "Output symbols", NULL },
        { "transparency-color", 't', 0, G_OPTION_ARG_CALLBACK, parse_transparency_color_arg, "Transparency color", NULL },
        { "transparency-threshold", '\0', 0, G_OPTION_ARG_DOUBLE, &options.transparency_threshold, "Transparency threshold", NULL },
        { NULL }
    };

    memset (&options, 0, sizeof (options));

    context = g_option_context_new ("[COMMAND] [OPTION...]");
    g_option_context_set_help_enabled (context, FALSE);
    g_option_context_add_main_entries (context, option_entries, NULL);

    options.executable_name = g_strdup ((*argv) [0]);

    /* Defaults */
    options.mode = detect_canvas_mode ();
    options.color_space = CHAFA_COLOR_SPACE_RGB;
    options.inc_sym = CHAFA_SYMBOLS_ALL;
    options.exc_sym = CHAFA_SYMBOLS_NONE;
    options.width = 80;
    options.height = 25;
    options.font_ratio = 1.0 / 2.0;
    options.quality = 5;
    options.transparency_color = 0x00000000;
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
	if (options.mode == CHAFA_CANVAS_MODE_SHAPES_WHITE_ON_BLACK)
        {
	    options.mode = CHAFA_CANVAS_MODE_SHAPES_BLACK_ON_WHITE;
            if (!options.transparency_color_set)
                options.transparency_color = 0x00ffffff;
        }
	else if (options.mode == CHAFA_CANVAS_MODE_INDEXED_WHITE_ON_BLACK)
        {
	    options.mode = CHAFA_CANVAS_MODE_INDEXED_BLACK_ON_WHITE;
            if (!options.transparency_color_set)
                options.transparency_color = 0x00ffffff;
        }
    }

    if (!options.preprocess_set)
    {
        options.preprocess = FALSE;

	if (options.mode == CHAFA_CANVAS_MODE_SHAPES_WHITE_ON_BLACK
	    || options.mode == CHAFA_CANVAS_MODE_SHAPES_BLACK_ON_WHITE
	    || options.mode == CHAFA_CANVAS_MODE_INDEXED_WHITE_ON_BLACK
	    || options.mode == CHAFA_CANVAS_MODE_INDEXED_BLACK_ON_WHITE
	    || options.mode == CHAFA_CANVAS_MODE_INDEXED_16)
        {
            options.preprocess = TRUE;
        }
    }

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

static void
textify (guint8 *pixels,
         gint src_width, gint src_height,
         gint dest_width, gint dest_height)
{
    ChafaCanvas *canvas;
    GString *gs;

    canvas = chafa_canvas_new (options.mode, dest_width, dest_height);

    chafa_canvas_set_color_space (canvas, options.color_space);
    chafa_canvas_set_include_symbols (canvas, options.inc_sym);
    chafa_canvas_set_exclude_symbols (canvas, options.exc_sym);
    chafa_canvas_set_transparency_color (canvas, options.transparency_color);
    if (options.transparency_threshold >= 0.0)
        chafa_canvas_set_transparency_threshold (canvas, options.transparency_threshold);
    chafa_canvas_set_quality (canvas, options.quality);

    chafa_canvas_paint_argb (canvas, pixels, src_width, src_height, src_width * 4);
    gs = chafa_canvas_build_gstring (canvas);
    fwrite (gs->str, sizeof (gchar), gs->len, stdout);
    fflush (stdout);
    g_string_free (gs, TRUE);

    chafa_canvas_unref (canvas);
}

static void
run (const gchar *filename)
{
    MagickWand *wand = NULL;
    gint src_width, src_height;
    gint dest_width, dest_height;
    guint8 *pixels;

    MagickWandGenesis();

    wand = NewMagickWand();
    MagickReadImage(wand, filename);
    auto_orient_image (wand);

    src_width = MagickGetImageWidth(wand);
    src_height = MagickGetImageHeight(wand);

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

    if (options.preprocess)
    {
	MagickModulateImage (wand, 100, 150, 100);
	MagickBrightnessContrastImage (wand, 0, 40);
    }

    pixels = g_malloc (src_width * src_height * 4);
    MagickExportImagePixels (wand,
			     0, 0,
			     src_width, src_height,
			     "RGBA",
			     CharPixel,
			     pixels);

#if 1
    /* Homes cursor so images can be concatenated to create animations.
     * FIXME: Make optional. */
    printf ("\x1b[0f");
#endif

    textify (pixels,
             src_width, src_height,
             dest_width, dest_height);
    g_free (pixels);

    /* Clean up */
    if (wand)
        wand = DestroyMagickWand (wand);
	
    MagickWandTerminus();
}

static int
run_all (GList *filenames)
{
    GList *l;

    for (l = filenames; l; l = g_list_next (l))
    {
        gchar *filename = l->data;
        run (filename);
    }

    return 0;
}

int
main (int argc, char *argv [])
{
    char *filename;
    int ret;

    setlocale (LC_ALL, "");

    if (!parse_options (&argc, &argv))
        exit (1);

    ret = run_all (options.args);
    return ret;
}
