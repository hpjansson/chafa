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
#include "font-loader.h"
#include "grid-layout.h"
#include "media-pipeline.h"
#include "named-colors.h"
#include "path-queue.h"
#include "placement-counter.h"
#include "util.h"

/* Include after glib.h for G_OS_WIN32 */
#ifdef G_OS_WIN32
# ifdef HAVE_WINDOWS_H
#  include <windows.h>
# endif
# include <wchar.h>
# include <io.h>
#endif

/* Maximum size of stack-allocated read/write buffer */
#define BUFFER_MAX 4096

#define ANIM_FPS_MAX 100000.0
#define FILE_DURATION_DEFAULT 0.0
#define SCALE_MAX 9999.0
#define CELL_EXTENT_AUTO_MAX 65535
#define PROBE_DURATION_DEFAULT 5.0

typedef enum
{
    TRISTATE_FALSE = 0,
    TRISTATE_TRUE,
    TRISTATE_AUTO
}
Tristate;

/* Dimensions are set to GRID_AUTO with --grid auto. This means the user
 * wants us to pick appropriate grid parameters based on the view size */
#define GRID_AUTO -2

typedef struct
{
    gchar *executable_name;

    gboolean show_help;
    gboolean show_version;
    gboolean skip_processing;

    GList *args;
    ChafaCanvasMode mode;
    gboolean dither_mode_set;
    ChafaColorExtractor color_extractor;
    ChafaColorSpace color_space;
    ChafaDitherMode dither_mode;
    ChafaPixelMode pixel_mode;
    gboolean pixel_mode_set;
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
    gboolean relative;
    gboolean relative_set;
    gboolean fit_to_width;
    gboolean grid_on;
    gboolean label;
    Tristate link_labels;
    gboolean use_unicode;
    ChafaAlign horiz_align;
    ChafaAlign vert_align;
    gint view_width, view_height;
    gint width, height;
    gint cell_width, cell_height;
    gint grid_width, grid_height;
    gint margin_bottom, margin_right;
    Tristate probe;
    gdouble probe_duration;
    gdouble scale;
    gdouble font_ratio;
    gint work_factor;
    gint optimization_level;
    gint n_threads;
    ChafaOptimizations optimizations;
    ChafaPassthrough passthrough;
    gboolean passthrough_set;
    guint32 fg_color;
    gboolean fg_color_set;
    guint32 bg_color;
    gboolean bg_color_set;
    gdouble transparency_threshold;
    gboolean transparency_threshold_set;
    gdouble file_duration_s;

    /* If > 0.0, override the framerate specified by the input file. */
    gdouble anim_fps;

    /* Applied after FPS is determined. If final value >= ANIM_FPS_MAX,
     * eliminate interframe delay altogether. */
    gdouble anim_speed_multiplier;

    Tristate use_exact_size;

    /* Automatically set if terminal size is detected and there is
     * zero bottom margin. */
    gboolean have_parking_row;

    /* Whether to perturb the options based on a seed read from the first
     * input file. This improves coverage when fuzzing. */
    gboolean fuzz_options;

    ChafaTermInfo *term_info;

    gboolean do_dump_detect;
}
GlobalOptions;

typedef struct
{
    gint width_cells, height_cells;
    gint width_pixels, height_pixels;
}
TermSize;

#ifdef G_OS_WIN32
/* Enable command line globbing on Windows.
 *
 * This is MinGW-specific. If you'd like it to work in MSVC, please get in
 * touch. There are a couple of glob-alikes floating around - for instance,
 * the one in MPV is portable and appropriately licensed. */
extern int _CRT_glob = 1;
#endif

static GlobalOptions options;
static TermSize detected_term_size;
static gboolean using_detected_size = FALSE;
static volatile sig_atomic_t interrupted_by_user = FALSE;

static PlacementCounter *placement_counter;

#ifdef HAVE_TERMIOS_H
static struct termios saved_termios;
#endif

#ifdef G_OS_WIN32
static UINT saved_console_output_cp;
static UINT saved_console_input_cp;
#endif

static ChafaTerm *term;
static PathQueue *global_path_queue;
static gint global_path_queue_n_stdin;
static gint global_n_path_streams;

#ifdef HAVE_SIGACTION
static void
sigint_handler (G_GNUC_UNUSED int sig)
{
    interrupted_by_user = TRUE;
}
#endif

static void
interruptible_usleep (gdouble us)
{
    while (us > 0.0 && !interrupted_by_user)
    {
        gdouble sleep_us = MIN (us, 50000.0);
        g_usleep (sleep_us);
        us -= sleep_us;
    }
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
    gchar **loaders = get_loader_names ();
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
                          G_STRINGIFY (PROBE_DURATION_DEFAULT) ".\n"
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
    "  -g, --grid=CxR     Lay out images in a grid of CxR columns/rows per screenful.\n"
    "                     C or R may be omitted, e.g. \"--grid 4\". Can be \"auto\".\n"
    "  -l, --label=BOOL   Labeling [on, off]. Prints filenames below images. Defaults\n"
    "                     to off.\n"
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

    "  $ chafa --scale max in.jpg                         # As big as will fit\n"
    "  $ chafa --clear --align mid,mid -d 5 *.gif         # A well-paced slideshow\n"
    "  $ chafa -f symbols --symbols ascii -c none in.png  # Old-school ASCII art\n\n"

    "If your OS comes with manual pages, you can type 'man chafa' for more.\n";

    g_print ("Usage:\n  %s [OPTION...] [FILE...]\n\n%s",
             options.executable_name,
             summary);
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

static Tristate
fuzz_seed_get_tristate (gconstpointer seed, gint seed_len, gint *ofs)
{
    const guchar *seed_u8 = seed;
    guint v = seed_u8 [(*ofs)++ % seed_len];

    return (v < 256 / 3) ? TRISTATE_FALSE
        : (v < (256 * 2) / 3) ? TRISTATE_TRUE
        : TRISTATE_AUTO;
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
fuzz_options_with_seed (GlobalOptions *opt, gconstpointer seed, gint seed_len)
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
fuzz_options_with_file (GlobalOptions *opt, const gchar *filename)
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
parse_tristate_token (const gchar *token, Tristate *value_out)
{
    gboolean success = FALSE;
    gboolean value_bool = FALSE;

    if (!g_ascii_strcasecmp (token, "auto")
        || !g_ascii_strcasecmp (token, "default"))
    {
        *value_out = TRISTATE_AUTO;
    }
    else
    {
        success = parse_boolean_token (token, &value_bool);
        if (!success)
            goto out;

        *value_out = value_bool ? TRISTATE_TRUE : TRISTATE_FALSE;
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

        options.probe = TRISTATE_TRUE;
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
        scale = SCALE_MAX;
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

    path_queue_push_stream (global_path_queue, value, "\n", 1);
    return TRUE;
}

static gboolean
parse_files0_arg (G_GNUC_UNUSED const gchar *option_name, const gchar *value, G_GNUC_UNUSED gpointer data, G_GNUC_UNUSED GError **error)
{
    global_n_path_streams++;

    if (!strcmp (value, "-"))
        global_path_queue_n_stdin++;

    path_queue_push_stream (global_path_queue, value, "\0", 1);
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
        width = height = (b ? GRID_AUTO : -1);
        options.grid_on = b;
    }
    else if (!g_ascii_strcasecmp (value, "auto"))
    {
        width = height = GRID_AUTO;
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
    FileMapping *file_mapping;
    FontLoader *font_loader;
    ChafaSymbolMap *temp_map = NULL;
    gunichar c;
    gpointer c_bitmap;
    gint width, height, rowstride;

    options.skip_processing = TRUE;

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

    temp_map = chafa_symbol_map_new ();

    while (font_loader_get_next_glyph (font_loader, &c, &c_bitmap, &width, &height))
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

    font_loader_destroy (font_loader);

    result = TRUE;

out:
    if (file_mapping)
        file_mapping_destroy (file_mapping);
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
tty_options_init (void)
{
    if (!options.polite)
    {
#ifdef HAVE_TERMIOS_H
        if (options.is_interactive)
        {
            struct termios t;

            tcgetattr (STDIN_FILENO, &saved_termios);
            t = saved_termios;
            t.c_lflag &= ~(ECHO | ICANON);
            tcsetattr (STDIN_FILENO, TCSANOW, &t);
        }
#endif

        if (options.mode != CHAFA_CANVAS_MODE_FGBG)
        {
            chafa_term_print_seq (term, CHAFA_TERM_SEQ_DISABLE_CURSOR, -1);
        }

        if (options.pixel_mode == CHAFA_PIXEL_MODE_SIXELS)
        {
            /* Most terminals should have sixel scrolling and advance-down enabled
             * by default, so we're not going to undo these later. */
            chafa_term_print_seq (term, CHAFA_TERM_SEQ_ENABLE_SIXEL_SCROLLING, -1);
            chafa_term_print_seq (term, CHAFA_TERM_SEQ_SET_SIXEL_ADVANCE_DOWN, -1);
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
            chafa_term_print_seq (term, CHAFA_TERM_SEQ_ENABLE_CURSOR, -1);
        }

#ifdef HAVE_TERMIOS_H
        if (options.is_interactive)
        {
            tcsetattr (STDIN_FILENO, TCSANOW, &saved_termios);
        }
#endif
    }
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

static gboolean
apply_passthrough_workarounds_tmux (void)
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

static gboolean
retire_passthrough_workarounds_tmux (void)
{
    gboolean result = FALSE;
    gchar *standard_output = NULL;
    gchar *standard_error = NULL;
    gchar *cmd;
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

    g_free (standard_output);
    g_free (standard_error);
    return result;
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

static gboolean
parse_options (int *argc, char **argv [])
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
    options.probe = TRISTATE_AUTO;
    options.probe_duration = PROBE_DURATION_DEFAULT;
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
    options.use_exact_size = TRISTATE_AUTO;
    options.cell_width = 10;
    options.cell_height = 20;
    options.label = FALSE;
    options.link_labels = TRISTATE_AUTO;
    options.use_unicode = TRUE;

    if (!g_option_context_parse (context, argc, argv, &error))
    {
        g_printerr ("%s: %s\n", options.executable_name, error->message);
        goto out;
    }

    /* Parser kludges */

    if (options.grid_on && options.grid_width == -1 && options.grid_height == -1)
    {
        options.grid_width = options.grid_height = GRID_AUTO;
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

    if ((options.probe == TRISTATE_TRUE
         || options.probe == TRISTATE_AUTO)
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
        options.view_width = CELL_EXTENT_AUTO_MAX;
    if (options.view_height < 0)
        options.view_height = CELL_EXTENT_AUTO_MAX;

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
            options.height = CELL_EXTENT_AUTO_MAX;

            /* Fit-to-width only makes sense if we respect the aspect ratio.
             * We could complain about infinities here, but let's be nice and
             * let it override --stretch instead. */
            options.stretch = FALSE;

            /* --fit-width implies --scale max */
            options.scale = SCALE_MAX;
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

    if (options.grid_width == GRID_AUTO || options.grid_height == GRID_AUTO)
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
        apply_passthrough_workarounds_tmux ();

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

    if (options.link_labels == TRISTATE_AUTO)
        options.link_labels = isatty (STDOUT_FILENO) ? TRISTATE_TRUE : TRISTATE_FALSE;

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
    else if (path_queue_get_length (global_path_queue) == 0)
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
        if (g_list_length (options.args) != 1 || path_queue_get_length (global_path_queue) != 0)
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
        options.scale = SCALE_MAX;
    }

    /* --stretch implies --scale max (same as --zoom) */
    if (options.stretch)
    {
        options.scale = SCALE_MAX;
    }

    /* `--exact-size on` overrides other sizing options */
    if (options.use_exact_size == TRISTATE_TRUE)
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

static void
pixel_to_cell_dimensions (gdouble scale,
                          gint cell_width, gint cell_height,
                          gint width, gint height,
                          gint *width_out, gint *height_out)
{
    /* Scale can't be zero or negative */
    scale = MAX (scale, 0.00001);

    /* Zero or negative cell dimensions -> presumably unknown, use 10x20 */
    if (cell_width < 1)
        cell_width = 10;
    if (cell_height < 1)
        cell_height = 20;

    if (width_out)
    {
        *width_out = ((int) (width * scale) + cell_width - 1) / cell_width;
        *width_out = MAX (*width_out, 1);
    }

    if (height_out)
    {
        *height_out = ((int) (height * scale) + cell_height - 1) / cell_height;
        *height_out = MAX (*height_out, 1);
    }
}

static void
write_gstring_to_stdout (GString *gs)
{
    chafa_term_write (term, gs->str, gs->len);
}

static void
write_gstrings_to_stdout (GString **gsa)
{
    gint i;

    for (i = 0; gsa [i]; i++)
        write_gstring_to_stdout (gsa [i]);
}

static void
write_pad_spaces (gint n)
{
    gchar buf [BUFFER_MAX];
    gint i;

    g_assert (n >= 0);

    n = MIN (n, BUFFER_MAX);
    for (i = 0; i < n; i++)
        buf [i] = ' ';

    chafa_term_write (term, buf, n);
}

static void
write_cursor_down_scroll_n (gint n)
{
    gchar buf [BUFFER_MAX];
    gchar *p0 = buf;

    if (n < 1)
        return;

    for ( ; n; n--)
    {
        p0 = chafa_term_info_emit_cursor_down_scroll (options.term_info, p0);
        if (p0 - buf + CHAFA_TERM_SEQ_LENGTH_MAX > BUFFER_MAX)
        {
            chafa_term_write (term, buf, p0 - buf);
            p0 = buf;
        }
    }

    chafa_term_write (term, buf, p0 - buf);
}

static void
write_vertical_spaces (gint n)
{
    gchar buf [BUFFER_MAX];

    if (n < 1)
        return;

    if (options.relative)
    {
        write_cursor_down_scroll_n (n);
        return;
    }

    memset (buf, '\n', MIN (n, BUFFER_MAX));

    for ( ; n > 0; n -= BUFFER_MAX)
        chafa_term_write (term, buf, MIN (n, BUFFER_MAX));
}

static void
write_image_prologue (const gchar *path,
                      gboolean is_first_file, gboolean is_first_frame,
                      gboolean is_animation, gint dest_height)
{
    /* Insert a blank line between files in continuous (!clear) mode */
    if (is_first_frame && !options.clear && !is_first_file)
        write_vertical_spaces (1 + (options.have_parking_row ? 0 : 1));

    if (options.clear)
    {
        if (is_first_frame)
        {
            /* Clear */
            chafa_term_print_seq (term, CHAFA_TERM_SEQ_CLEAR, -1);
        }

        /* Home cursor between frames */
        chafa_term_print_seq (term, CHAFA_TERM_SEQ_CURSOR_TO_TOP_LEFT, -1);
    }

    /* Insert space for vertical alignment */
    if (options.clear || is_first_frame)
        write_vertical_spaces (options.vert_align == CHAFA_ALIGN_START ? 0
                               : options.vert_align == CHAFA_ALIGN_CENTER
                               ? ((options.view_height - dest_height - options.margin_bottom) / 2)
                               : (options.view_height - dest_height - options.margin_bottom));

    if (!options.clear && is_animation && is_first_frame)
    {
        /* Start animation: reserve space and save image origin */
        write_cursor_down_scroll_n (dest_height);
        chafa_term_print_seq (term, CHAFA_TERM_SEQ_CURSOR_UP, dest_height, -1);
        chafa_term_print_seq (term, CHAFA_TERM_SEQ_SAVE_CURSOR_POS, -1);

        /* Print label before animation starts */
        if (options.label)
        {
            chafa_term_print_seq (term, CHAFA_TERM_SEQ_CURSOR_DOWN, dest_height, -1);
            path_print_label (term, path, options.horiz_align, options.view_width,
                              options.use_unicode,
                              options.link_labels != TRISTATE_FALSE ? TRUE : FALSE);
            chafa_term_print_seq (term, CHAFA_TERM_SEQ_RESTORE_CURSOR_POS, -1);
        }
    }
    else if (!options.clear && !is_first_frame)
    {
        /* Subsequent animation frames: cursor to image origin */
        chafa_term_print_seq (term, CHAFA_TERM_SEQ_RESTORE_CURSOR_POS, -1);
    }
}

static void
write_image_epilogue (const gchar *path, gboolean is_animation, gint dest_width)
{
    gint left_space;

    left_space = options.horiz_align == CHAFA_ALIGN_START ? 0
        : options.horiz_align == CHAFA_ALIGN_CENTER
        ? ((options.view_width - dest_width - options.margin_right) / 2)
        : (options.view_width - dest_width - options.margin_right);

    /* These modes leave cursor to the right of the bottom row */
    if (options.pixel_mode == CHAFA_PIXEL_MODE_SYMBOLS
        || options.pixel_mode == CHAFA_PIXEL_MODE_KITTY
        || options.pixel_mode == CHAFA_PIXEL_MODE_ITERM2)
    {
        if (options.have_parking_row)
        {
            write_vertical_spaces (1);
        }
        else if (!options.relative)
        {
            /* Windows needs an explicit CR in absolute mode */
            chafa_term_write (term, "\r", 1);
        }

        if (options.relative)
            chafa_term_print_seq (term, CHAFA_TERM_SEQ_CURSOR_LEFT,
                                  left_space + dest_width, -1);

        if (options.label)
        {
            if (!is_animation)
            {
                path_print_label (term, path, options.horiz_align, options.view_width,
                                  options.use_unicode,
                                  options.link_labels != TRISTATE_FALSE ? TRUE : FALSE);

                if (options.relative)
                    chafa_term_print_seq (term, CHAFA_TERM_SEQ_CURSOR_LEFT,
                                          options.view_width, -1);
            }

            write_vertical_spaces (1);
        }
    }
    else /* CHAFA_PIXEL_MODE_SIXELS */
    {
        /* Sixel mode leaves the cursor in the leftmost column of the final band,
         * but some terminals have a quirk where they'll leave it on the row
         * below. */

        if (!(chafa_term_info_get_quirks (chafa_term_get_term_info (term))
              & CHAFA_TERM_QUIRK_SIXEL_OVERSHOOT)
            && options.have_parking_row)
            write_vertical_spaces (1);

        if (options.relative && left_space > 0)
            chafa_term_print_seq (term, CHAFA_TERM_SEQ_CURSOR_LEFT,
                                  left_space, -1);

        if (options.label)
        {
            if (!is_animation)
            {
                if (!options.relative)
                    chafa_term_write (term, "\r", 1);

                path_print_label (term, path, options.horiz_align, options.view_width,
                                  options.use_unicode,
                                  options.link_labels != TRISTATE_FALSE ? TRUE : FALSE);

                if (options.relative)
                    chafa_term_print_seq (term, CHAFA_TERM_SEQ_CURSOR_LEFT,
                                          options.view_width, -1);
            }

            write_vertical_spaces (1);
        }
    }
}

/* Write out the image data, possibly centering it */
static void
write_image (GString **gsa, gint dest_width)
{
    gint left_space;

    left_space = options.horiz_align == CHAFA_ALIGN_START ? 0
        : options.horiz_align == CHAFA_ALIGN_CENTER
        ? ((options.view_width - dest_width - options.margin_right) / 2)
        : (options.view_width - dest_width - options.margin_right);
    left_space = MAX (left_space, 0);

    /* Indent top left corner: Common for all modes */

    if (options.relative && left_space > 0)
        chafa_term_print_seq (term, CHAFA_TERM_SEQ_CURSOR_RIGHT, left_space, -1);
    else
        write_pad_spaces (left_space);

    if (options.pixel_mode == CHAFA_PIXEL_MODE_SYMBOLS)
    {
        gint i;

        /* Indent subsequent rows: Symbols mode only */

        for (i = 0; gsa [i]; i++)
        {
            write_gstring_to_stdout (gsa [i]);

            if (gsa [i + 1])
            {
                if (options.relative)
                {
                    chafa_term_print_seq (term, CHAFA_TERM_SEQ_CURSOR_LEFT,
                                          left_space + dest_width, -1);
                    chafa_term_print_seq (term, CHAFA_TERM_SEQ_CURSOR_DOWN_SCROLL, -1);

                    if (left_space > 0)
                        chafa_term_print_seq (term, CHAFA_TERM_SEQ_CURSOR_RIGHT, left_space, -1);
                }
                else
                {
                    chafa_term_write (term, "\n", 1);
                    write_pad_spaces (left_space);
                }
            }
        }
    }
    else
    {
        write_gstrings_to_stdout (gsa);
    }
}

static ChafaCanvasConfig *
build_config (gint dest_width, gint dest_height, gboolean is_animation)
{
    ChafaCanvasConfig *config;

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
    chafa_canvas_config_set_passthrough (config, options.passthrough);

    /* With Kitty and sixels, animation frames should have an opaque background.
     * Otherwise, previous frames will show through transparent areas. */
    if (is_animation
        && (options.pixel_mode == CHAFA_PIXEL_MODE_KITTY
            || options.pixel_mode == CHAFA_PIXEL_MODE_SIXELS)
        && !options.transparency_threshold_set)
        chafa_canvas_config_set_transparency_threshold (config, 1.0f);
    else if (options.transparency_threshold >= 0.0)
        chafa_canvas_config_set_transparency_threshold (config, options.transparency_threshold);

    if (options.cell_width > 0 && options.cell_height > 0)
        chafa_canvas_config_set_cell_geometry (config, options.cell_width, options.cell_height);

    chafa_canvas_config_set_symbol_map (config, options.symbol_map);
    chafa_canvas_config_set_fill_symbol_map (config, options.fill_symbol_map);

    /* Work switch takes values [1..9], we normalize to [0.0..1.0] to
     * get the work factor. */
    chafa_canvas_config_set_work_factor (config, (options.work_factor - 1) / 8.0f);

    chafa_canvas_config_set_optimizations (config, options.optimizations);
    return config;
}

static ChafaCanvas *
build_canvas (ChafaPixelType pixel_type, const guint8 *pixels,
              gint src_width, gint src_height, gint src_rowstride,
              const ChafaCanvasConfig *config,
              gint placement_id,
              ChafaTuck tuck)
{
    ChafaFrame *frame;
    ChafaImage *image;
    ChafaPlacement *placement;
    ChafaCanvas *canvas;

    canvas = chafa_canvas_new (config);
    frame = chafa_frame_new_borrow (pixels, pixel_type,
                                    src_width, src_height, src_rowstride);
    image = chafa_image_new ();
    chafa_image_set_frame (image, frame);

    placement = chafa_placement_new (image, placement_id);
    chafa_placement_set_tuck (placement, tuck);
    chafa_placement_set_halign (placement, options.horiz_align);
    chafa_placement_set_valign (placement, options.vert_align);
    chafa_canvas_set_placement (canvas, placement);

    chafa_placement_unref (placement);
    chafa_image_unref (image);
    chafa_frame_unref (frame);
    return canvas;
}

typedef enum
{
    FILE_FAILED,
    FILE_WAS_STILL,
    FILE_WAS_ANIMATION
}
RunResult;

/* Prescaling is for structured formats like SVG where the intrinsic size may be
 * too small or too large and we'd rather rasterize to something closer to our
 * final output size. */
static void
calc_prescale_size_px (gint *prescale_width_out, gint *prescale_height_out)
{
    gint cell_width_px, cell_height_px;

    if (options.cell_width <= 0 || options.cell_height <= 0)
    {
        cell_width_px = 10;
        cell_height_px = 20;
    }
    else
    {
        cell_width_px = options.cell_width;
        cell_height_px = options.cell_height;
    }

    *prescale_width_out = cell_width_px
        * (options.width > 0 ? options.width : options.view_width);
    *prescale_height_out = cell_height_px
        * (options.height > 0 ? options.height : options.view_height);

    *prescale_width_out = MAX (*prescale_width_out, 160);
    *prescale_height_out = MAX (*prescale_height_out, 160);
}

static RunResult
run_generic (const gchar *filename, MediaLoader *media_loader,
             gboolean is_first_file, gboolean is_first_frame)
{
    gboolean is_animation = FALSE;
    gdouble anim_duration_s = options.file_duration_s >= 0.0 ? options.file_duration_s : G_MAXDOUBLE;
    gdouble anim_elapsed_s = 0.0;
    GTimer *timer;
    gint loop_n = 0;
    GString **gsa;
    gint placement_id = -1;
    gint frame_count = 0;
    RunResult result = FILE_FAILED;
    gint dest_width = 0, dest_height = 0;
    GError *error = NULL;

    timer = g_timer_new ();

    if (interrupted_by_user)
        goto out;

    if (options.pixel_mode == CHAFA_PIXEL_MODE_KITTY
        && options.passthrough != CHAFA_PASSTHROUGH_NONE)
    {
        if (!placement_counter)
            placement_counter = placement_counter_new ();

        placement_id = placement_counter_get_next_id (placement_counter);
    }

    is_animation = options.animate ? media_loader_get_is_animation (media_loader) : FALSE;
    result = is_animation ? FILE_WAS_ANIMATION : FILE_WAS_STILL;

    do
    {
        gboolean have_frame;

        /* Outer loop repeats animation if desired */

        media_loader_goto_first_frame (media_loader);

        for (have_frame = TRUE;
             have_frame && !interrupted_by_user && (loop_n == 0 || anim_elapsed_s < anim_duration_s);
             have_frame = media_loader_goto_next_frame (media_loader))
        {
            gdouble elapsed_ms, remain_ms;
            gint delay_ms;
            ChafaPixelType pixel_type;
            gint src_width, src_height, src_rowstride;
            gint uncorrected_src_width, uncorrected_src_height;
            gint virt_src_width, virt_src_height;
            const guint8 *pixels;
            ChafaCanvasConfig *config;
            ChafaCanvas *canvas;
            ChafaTuck tuck;

            g_timer_start (timer);

            if (options.use_exact_size == TRISTATE_TRUE)
            {
                /* True */
                tuck = CHAFA_TUCK_SHRINK_TO_FIT;
            }
            else
            {
                /* False/auto */
                if (options.stretch)
                {
                    tuck = CHAFA_TUCK_STRETCH;
                }
                else
                {
                    tuck = CHAFA_TUCK_FIT;
                }
            }

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

            /* Hack to work around the fact that chafa_calc_canvas_geometry() doesn't
             * support arbitrary scaling. Instead, we manipulate the source size to
             * achieve the desired effect. */
            if (using_detected_size && options.scale < SCALE_MAX - 0.1)
            {
                pixel_to_cell_dimensions (options.scale,
                                          options.cell_width, options.cell_height,
                                          src_width, src_height,
                                          &uncorrected_src_width, &uncorrected_src_height);

                virt_src_width = uncorrected_src_width;
                if (options.cell_width > 0 && options.cell_height > 0)
                    virt_src_height = uncorrected_src_height / options.font_ratio;
                else
                    virt_src_height = uncorrected_src_height;

                virt_src_height = MAX (virt_src_height, 1);
            }
            else
            {
                virt_src_width = uncorrected_src_width = src_width;
                virt_src_height = uncorrected_src_height = src_height;
            }

            if (options.use_exact_size == TRISTATE_TRUE)
            {
                dest_width = virt_src_width;
                dest_height = virt_src_height;
            }
            else
            {
                dest_width = options.width;
                dest_height = options.height;
            }

            chafa_calc_canvas_geometry (virt_src_width,
                                        virt_src_height,
                                        &dest_width,
                                        &dest_height,
                                        options.font_ratio,
                                        options.scale >= SCALE_MAX - 0.1 ? TRUE : FALSE,
                                        options.stretch);

            if (options.use_exact_size == TRISTATE_AUTO
                && dest_width == uncorrected_src_width
                && dest_height == uncorrected_src_height)
            {
                tuck = (options.pixel_mode == CHAFA_PIXEL_MODE_SYMBOLS
                        ? CHAFA_TUCK_FIT : CHAFA_TUCK_SHRINK_TO_FIT);
            }

#if 0
            /* The size calculations are too convoluted, so we may need this to
             * debug --exact-size. */
            g_printerr ("src=(%dx%d) unc=(%dx%d) virt=(%dx%d) dest=(%dx%d)\n",
                        src_width, src_height,
                        uncorrected_src_width, uncorrected_src_height,
                        virt_src_width, virt_src_height,
                        dest_width, dest_height);
#endif

            config = build_config (dest_width, dest_height, is_animation);
            canvas = build_canvas (pixel_type, pixels,
                                   src_width, src_height, src_rowstride, config,
                                   placement_id >= 0 ? placement_id + ((frame_count++) % 2) : -1,
                                   tuck);

            chafa_canvas_print_rows (canvas, options.term_info, &gsa, NULL);

            write_image_prologue (filename, is_first_file, is_first_frame, is_animation, dest_height);
            write_image (gsa, dest_width);

            /* No inter-frame epilogue in animations; this prevents unwanted
             * scrolling when we get the sixel overshoot quirk wrong (#255). */
            if (!is_animation)
                write_image_epilogue (filename, is_animation, dest_width);

            chafa_term_flush (term);
            chafa_free_gstring_array (gsa);
            chafa_canvas_unref (canvas);
            chafa_canvas_config_unref (config);

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
                    interruptible_usleep (remain_ms * 1000.0);

                anim_elapsed_s += MAX (elapsed_ms, delay_ms) / 1000.0;
            }

            is_first_frame = FALSE;

            if (!is_animation)
                break;
        }

        loop_n++;
    }
    while (is_animation && !interrupted_by_user
           && !options.watch && anim_elapsed_s < anim_duration_s);

    if (is_animation)
        write_image_epilogue (filename, is_animation, dest_width);

out:
    /* We need two IDs per animation in order to do flicker-free flips. If the
     * final frame got the higher ID, increment the global counter so the next
     * image doesn't clobber it. */
    if (placement_id >= 0 && !(frame_count % 2))
        placement_id = placement_counter_get_next_id (placement_counter);

    g_timer_destroy (timer);
    g_clear_error (&error);
    return result;
}

static RunResult
run (const gchar *path, MediaLoader *loader,
     gboolean is_first_file, gboolean is_first_frame)
{
    return run_generic (path, loader, is_first_file, is_first_frame);
}

static int
run_watch (const gchar *filename)
{
    GTimer *timer;
    gboolean is_first_frame = TRUE;
    gdouble duration_s = options.file_duration_s >= 0.0 ? options.file_duration_s : G_MAXDOUBLE;
    gint prescale_width, prescale_height;

    calc_prescale_size_px (&prescale_width, &prescale_height);

    timer = g_timer_new ();

    for ( ; !interrupted_by_user; )
    {
        struct stat sbuf;

        if (!stat (filename, &sbuf))
        {
            MediaLoader *media_loader;

            /* Sadly we can't rely on timestamps to tell us when to reload
             * the file, since they can take way too long to update. */

            media_loader = media_loader_new (filename, prescale_width, prescale_height, NULL);
            if (media_loader)
                run (filename, media_loader, TRUE, is_first_frame);
            is_first_frame = FALSE;

            g_usleep (10000);
        }
        else
        {
            /* Don't hammer the path if the file is temporarily gone */

            g_usleep (250000);
        }

        if (g_timer_elapsed (timer, NULL) > duration_s)
            break;
    }

    g_timer_destroy (timer);
    return 0;
}

static int
run_all (PathQueue *path_queue)
{
    MediaPipeline *pipeline;
    gdouble still_duration_s = options.file_duration_s > 0.0 ? options.file_duration_s : 0.0;
    gint n_processed = 0;
    gint n_failed = 0;
    gint prescale_width, prescale_height;

    calc_prescale_size_px (&prescale_width, &prescale_height);
    pipeline = media_pipeline_new (path_queue, prescale_width, prescale_height);

    while (!interrupted_by_user)
    {
        gchar *path = NULL;
        MediaLoader *media_loader = NULL;
        GError *error = NULL;
        RunResult result;

        if (!media_pipeline_pop (pipeline, &path, &media_loader, &error))
            break;

        if (!media_loader)
        {
            if (error)
            {
                g_printerr ("%s: Failed to open '%s': %s\n",
                            options.executable_name,
                            path ? path : "?",
                            error->message);
                g_error_free (error);
            }

            g_free (path);
            continue;
        }

        result = run (path, media_loader, n_processed > 0 ? FALSE : TRUE, TRUE);

        n_processed++;
        if (result == FILE_FAILED)
            n_failed++;

        if (result == FILE_WAS_STILL && still_duration_s > 0.0)
        {
            interruptible_usleep (still_duration_s * 1000000.0);
        }

        media_loader_destroy (media_loader);
        g_free (path);
    }

    /* Emit linefeed after last image when cursor was not in parking row */
    if (!options.have_parking_row)
        chafa_term_write (term, "\n", 1);

    /* Zero files processed is not a failure, since we may be processing an
     * empty file list. */
    return (n_failed > 0 && n_failed == n_processed) ? 2 : (n_failed > 0) ? 1 : 0;
}

static gint
run_grid (PathQueue *path_queue)
{
    ChafaCanvasConfig *canvas_config;
    GridLayout *grid_layout;

    /* The prototype canvas' size isn't used for anything; set it to a legal value */
    canvas_config = build_config (1, 1, FALSE);

    grid_layout = grid_layout_new ();
    grid_layout_set_view_size (grid_layout, options.width, options.height);
    grid_layout_set_grid_size (grid_layout, options.grid_width, options.grid_height);
    grid_layout_set_canvas_config (grid_layout, canvas_config);
    grid_layout_set_term_info (grid_layout, options.term_info);
    grid_layout_set_align (grid_layout, options.horiz_align, options.vert_align);
    grid_layout_set_tuck (grid_layout,
                          options.use_exact_size == TRISTATE_TRUE
                          ? CHAFA_TUCK_SHRINK_TO_FIT
                          : (options.stretch ? CHAFA_TUCK_STRETCH : CHAFA_TUCK_FIT));
    grid_layout_set_print_labels (grid_layout, options.label);
    grid_layout_set_link_labels (grid_layout, options.link_labels != TRISTATE_FALSE ? TRUE : FALSE);
    grid_layout_set_use_unicode (grid_layout, options.use_unicode);
    grid_layout_set_path_queue (grid_layout, path_queue);

    while (!interrupted_by_user && grid_layout_print_chunk (grid_layout, term))
        ;

    chafa_canvas_config_unref (canvas_config);
    grid_layout_destroy (grid_layout);
    return 0;
}

static void
proc_init (void)
{
#ifdef HAVE_SIGACTION
    struct sigaction sa = { 0 };

    sa.sa_handler = sigint_handler;
    sa.sa_flags = SA_RESETHAND;

    sigaction (SIGINT, &sa, NULL);
#endif

#ifdef G_OS_WIN32
    setmode (fileno (stdin), O_BINARY);
    setmode (fileno (stdout), O_BINARY);
#endif

    /* Must do this early. Buffer size probably doesn't matter */
    setvbuf (stdout, NULL, _IOFBF, 32768);

    setlocale (LC_ALL, "");

    /* Chafa may create and destroy GThreadPools multiple times while rendering
     * an image. This reduces thread churn and saves a decent amount of CPU. */
    g_thread_pool_set_max_unused_threads (-1);

    term = chafa_term_get_default ();
    global_path_queue = path_queue_new ();
}

int
main (int argc, char *argv [])
{
    int ret = 0;
    
    proc_init ();

    if (!parse_options (&argc, &argv))
        exit (2);

    if (options.args)
    {
        path_queue_push_path_list_steal (global_path_queue, options.args);
        options.args = NULL;
    }

    /* --version and --help can skip all the init/deinit stuff */
    if (options.skip_processing
        || path_queue_get_length (global_path_queue) == 0)
        goto out;

    tty_options_init ();

    if (options.grid_width > 0 || options.grid_height > 0)
    {
        ret = run_grid (global_path_queue);
    }
    else if (options.watch)
    {
        gchar *path = path_queue_try_pop (global_path_queue);

        if (path)
        {
            ret = run_watch (path);
            g_free (path);
        }
    }
    else
    {
        ret = run_all (global_path_queue);
    }

    tty_options_deinit ();
    chafa_term_flush (term);

    retire_passthrough_workarounds_tmux ();

out:
    if (placement_counter)
        placement_counter_destroy (placement_counter);

    if (options.symbol_map)
        chafa_symbol_map_unref (options.symbol_map);
    if (options.fill_symbol_map)
        chafa_symbol_map_unref (options.fill_symbol_map);
    if (options.term_info)
        chafa_term_info_unref (options.term_info);

    path_queue_unref (global_path_queue);
    return ret;
}
