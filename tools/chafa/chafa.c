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

#include <string.h>  /* strspn, strlen, strcmp, strncmp, memset */
#include <locale.h>  /* setlocale */
#include <sys/types.h>  /* open */
#include <sys/stat.h>  /* stat */
#include <fcntl.h>  /* open */
#include <unistd.h>  /* STDOUT_FILENO */
#include <stdlib.h>  /* exit */
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>  /* ioctl */
#endif
#ifdef HAVE_SIGACTION
# include <signal.h>  /* sigaction */
#endif
#ifdef HAVE_TERMIOS_H
# include <termios.h>  /* tcgetattr, tcsetattr */
#endif

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

/* Maximum size of stack-allocated read/write buffer */
#define BUFFER_MAX 4096

#ifdef G_OS_WIN32
/* Enable command line globbing on Windows.
 *
 * This is MinGW-specific. If you'd like it to work in MSVC, please get in
 * touch. There are a couple of glob-alikes floating around - for instance,
 * the one in MPV is portable and appropriately licensed. */
extern int _CRT_glob = 1;
#endif

static volatile sig_atomic_t interrupted_by_user = FALSE;
static ChiclePlacementCounter *placement_counter;

#ifdef HAVE_TERMIOS_H
static struct termios saved_termios;
#endif

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
            chicle_path_print_label (term, path, options.horiz_align, options.view_width,
                                     options.use_unicode,
                                     options.link_labels != CHICLE_TRISTATE_FALSE ? TRUE : FALSE);
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
                chicle_path_print_label (term, path, options.horiz_align, options.view_width,
                                         options.use_unicode,
                                         options.link_labels != CHICLE_TRISTATE_FALSE ? TRUE : FALSE);

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

                chicle_path_print_label (term, path, options.horiz_align, options.view_width,
                                         options.use_unicode,
                                         options.link_labels != CHICLE_TRISTATE_FALSE ? TRUE : FALSE);

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
run_generic (const gchar *filename, ChicleMediaLoader *media_loader,
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
            placement_counter = chicle_placement_counter_new ();

        placement_id = chicle_placement_counter_get_next_id (placement_counter);
    }

    is_animation = options.animate ? chicle_media_loader_get_is_animation (media_loader) : FALSE;
    result = is_animation ? FILE_WAS_ANIMATION : FILE_WAS_STILL;

    do
    {
        gboolean have_frame;

        /* Outer loop repeats animation if desired */

        chicle_media_loader_goto_first_frame (media_loader);

        for (have_frame = TRUE;
             have_frame && !interrupted_by_user && (loop_n == 0 || anim_elapsed_s < anim_duration_s);
             have_frame = chicle_media_loader_goto_next_frame (media_loader))
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

            if (options.use_exact_size == CHICLE_TRISTATE_TRUE)
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

            pixels = chicle_media_loader_get_frame_data (media_loader,
                                                         &pixel_type,
                                                         &src_width,
                                                         &src_height,
                                                         &src_rowstride);
            /* FIXME: This shouldn't happen -- but if it does, our
             * options for handling it gracefully here aren't great.
             * Needs refactoring. */
            if (!pixels)
                break;

            delay_ms = chicle_media_loader_get_frame_delay (media_loader);

            /* Hack to work around the fact that chafa_calc_canvas_geometry() doesn't
             * support arbitrary scaling. Instead, we manipulate the source size to
             * achieve the desired effect. */
            if (using_detected_size && options.scale < CHICLE_SCALE_MAX - 0.1)
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

            if (options.use_exact_size == CHICLE_TRISTATE_TRUE)
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
                                        options.scale >= CHICLE_SCALE_MAX - 0.1 ? TRUE : FALSE,
                                        options.stretch);

            if (options.use_exact_size == CHICLE_TRISTATE_AUTO
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

                if (remain_ms > 0.0001 && 1000.0 / (gdouble) remain_ms < CHICLE_ANIM_FPS_MAX)
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
        placement_id = chicle_placement_counter_get_next_id (placement_counter);

    g_timer_destroy (timer);
    g_clear_error (&error);
    return result;
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
            ChicleMediaLoader *media_loader;

            /* Sadly we can't rely on timestamps to tell us when to reload
             * the file, since they can take way too long to update. */

            media_loader = chicle_media_loader_new (filename, prescale_width, prescale_height, NULL);
            if (media_loader)
            {
                run_generic (filename, media_loader, TRUE, is_first_frame);
                chicle_media_loader_destroy (media_loader);
            }
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
run_vertical (ChiclePathQueue *path_queue)
{
    ChicleMediaPipeline *pipeline;
    gdouble still_duration_s = options.file_duration_s > 0.0 ? options.file_duration_s : 0.0;
    gint n_processed = 0;
    gint n_failed = 0;
    gint prescale_width, prescale_height;

    calc_prescale_size_px (&prescale_width, &prescale_height);
    pipeline = chicle_media_pipeline_new (path_queue, prescale_width, prescale_height);
    chicle_media_pipeline_set_want_loader (pipeline, TRUE);
    chicle_media_pipeline_set_want_output (pipeline, FALSE);

    while (!interrupted_by_user)
    {
        gchar *path = NULL;
        ChicleMediaLoader *media_loader = NULL;
        GError *error = NULL;
        RunResult result;

        if (!chicle_media_pipeline_pop (pipeline, &path, &media_loader, NULL, &error))
            break;

        n_processed++;

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
            n_failed++;
            continue;
        }

        result = run_generic (path, media_loader, n_processed > 1 ? FALSE : TRUE, TRUE);
        if (result == FILE_FAILED)
            n_failed++;

        if (result == FILE_WAS_STILL && still_duration_s > 0.0)
        {
            interruptible_usleep (still_duration_s * 1000000.0);
        }

        chicle_media_loader_destroy (media_loader);
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
run_grid (ChiclePathQueue *path_queue)
{
    ChafaCanvasConfig *canvas_config;
    ChicleGridLayout *grid_layout;

    /* The prototype canvas' size isn't used for anything; set it to a legal value */
    canvas_config = build_config (1, 1, FALSE);

    grid_layout = chicle_grid_layout_new ();
    chicle_grid_layout_set_view_size (grid_layout, options.width, options.height);
    chicle_grid_layout_set_grid_size (grid_layout, options.grid_width, options.grid_height);
    chicle_grid_layout_set_canvas_config (grid_layout, canvas_config);
    chicle_grid_layout_set_term_info (grid_layout, options.term_info);
    chicle_grid_layout_set_align (grid_layout, options.horiz_align, options.vert_align);
    chicle_grid_layout_set_tuck (grid_layout,
                                 options.use_exact_size == CHICLE_TRISTATE_TRUE
                                 ? CHAFA_TUCK_SHRINK_TO_FIT
                                 : (options.stretch ? CHAFA_TUCK_STRETCH : CHAFA_TUCK_FIT));
    chicle_grid_layout_set_print_labels (grid_layout, options.label);
    chicle_grid_layout_set_link_labels (grid_layout, options.link_labels != CHICLE_TRISTATE_FALSE ? TRUE : FALSE);
    chicle_grid_layout_set_use_unicode (grid_layout, options.use_unicode);
    chicle_grid_layout_set_path_queue (grid_layout, path_queue);

    while (!interrupted_by_user && chicle_grid_layout_print_chunk (grid_layout, term))
        ;

    chafa_canvas_config_unref (canvas_config);
    chicle_grid_layout_destroy (grid_layout);
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
    global_path_queue = chicle_path_queue_new ();
}

int
main (int argc, char *argv [])
{
    int ret = 0;

    proc_init ();

    if (!chicle_parse_options (&argc, &argv))
        exit (2);

    if (options.args)
    {
        chicle_path_queue_push_path_list_steal (global_path_queue, options.args);
        options.args = NULL;
    }

    /* --version and --help can skip all the init/deinit stuff */
    if (options.skip_processing
        || chicle_path_queue_get_length (global_path_queue) == 0)
        goto out;

    tty_options_init ();

    if (options.grid_width > 0 || options.grid_height > 0)
    {
        ret = run_grid (global_path_queue);
    }
    else if (options.watch)
    {
        gchar *path = chicle_path_queue_try_pop (global_path_queue);

        if (path)
        {
            ret = run_watch (path);
            g_free (path);
        }
    }
    else
    {
        ret = run_vertical (global_path_queue);
    }

    tty_options_deinit ();
    chafa_term_flush (term);

    chicle_retire_passthrough_workarounds_tmux ();

out:
    if (placement_counter)
        chicle_placement_counter_destroy (placement_counter);

    if (options.symbol_map)
        chafa_symbol_map_unref (options.symbol_map);
    if (options.fill_symbol_map)
        chafa_symbol_map_unref (options.fill_symbol_map);
    if (options.term_info)
        chafa_term_info_unref (options.term_info);

    chicle_path_queue_unref (global_path_queue);
    return ret;
}
