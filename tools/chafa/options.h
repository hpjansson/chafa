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

#ifndef __OPTIONS_H__
#define __OPTIONS_H__

#include "config.h"

G_BEGIN_DECLS

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
#include "named-colors.h"

/* Include after glib.h for G_OS_WIN32 */
#ifdef G_OS_WIN32
# ifdef HAVE_WINDOWS_H
#  include <windows.h>
# endif
# include <wchar.h>
# include <io.h>
#endif

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

extern GlobalOptions options;
extern TermSize detected_term_size;
extern gboolean using_detected_size;
extern PathQueue *global_path_queue;
extern gint global_path_queue_n_stdin;
extern gint global_n_path_streams;
extern ChafaTerm *term;

gboolean parse_options (int *argc, char **argv []);
gboolean apply_passthrough_workarounds_tmux (void);
gboolean retire_passthrough_workarounds_tmux (void);

G_END_DECLS

#endif /* __OPTIONS_H__ */
