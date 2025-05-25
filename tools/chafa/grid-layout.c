/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2024-2025 Hans Petter Jansson
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
#include <chafa.h>
#include "grid-layout.h"
#include "media-loader.h"
#include "util.h"

#define MAX_COLS 1024

struct GridLayout
{
    gint view_width, view_height;
    gint n_cols, n_rows;
    ChafaCanvasConfig *canvas_config;
    ChafaTermInfo *term_info;
    ChafaAlign halign;
    ChafaAlign valign;
    ChafaTuck tuck;
    GList *paths, *next_path;
    gint n_items, next_item;
    guint finished_push : 1;
    guint finished_chunks : 1;
    guint print_labels : 1;
    guint use_unicode : 1;
};

static void
update_geometry (GridLayout *grid)
{
    gint view_width, view_height;
    gint n_cols, n_rows;
    gint item_width, item_height;
    gint cell_width_px, cell_height_px;
    gint font_ratio;

    if (!grid->canvas_config)
        return;

    chafa_canvas_config_get_cell_geometry (grid->canvas_config, &cell_width_px, &cell_height_px);
    if (cell_width_px < 1 || cell_height_px < 1)
    {
        cell_width_px = 10;
        cell_height_px = 20;
    }

    view_width = MAX (grid->view_width, 1);
    view_height = MAX (grid->view_height, 1);
    n_cols = grid->n_cols;
    n_rows = grid->n_rows;

    if (n_cols < 1 && n_rows < 1)
        n_cols = n_rows = 1;

    /* If one dimension is not provided, make square tiles */

    if (n_cols < 1)
    {
        item_height = view_height / n_rows - 1;
        item_width = (item_height * cell_height_px) / cell_width_px;
    }
    else if (n_rows < 1)
    {
        item_width = view_width / n_cols - 1;
        item_height = (item_width * cell_width_px) / cell_height_px;
    }
    else
    {
        item_width = view_width / n_cols - 1;
        item_height = view_height / n_rows - 1;
    }

    item_width = MAX (item_width, 1);
    item_height = MAX (item_height, 1);

    if (grid->n_cols < 1)
    {
        grid->n_cols = MIN (view_width / (item_width + 1), MAX_COLS);
        grid->n_cols = MAX (grid->n_cols, 1);
    }

    chafa_canvas_config_set_geometry (grid->canvas_config, item_width, item_height);
}

static ChafaCanvas *
build_canvas (ChafaPixelType pixel_type, const guint8 *pixels,
              gint src_width, gint src_height, gint src_rowstride,
              const ChafaCanvasConfig *config,
              gint placement_id,
              ChafaAlign halign,
              ChafaAlign valign,
              ChafaTuck tuck)
{
    ChafaFrame *frame;
    ChafaImage *image;
    ChafaPlacement *placement;
    ChafaCanvas *canvas;

    canvas = chafa_canvas_new (config);
    frame = chafa_frame_new_borrow ((gpointer) pixels, pixel_type,
                                    src_width, src_height, src_rowstride);
    image = chafa_image_new ();
    chafa_image_set_frame (image, frame);

    placement = chafa_placement_new (image, placement_id);
    chafa_placement_set_tuck (placement, tuck);
    chafa_placement_set_halign (placement, halign);
    chafa_placement_set_valign (placement, valign);
    chafa_canvas_set_placement (canvas, placement);

    chafa_placement_unref (placement);
    chafa_image_unref (image);
    chafa_frame_unref (frame);
    return canvas;
}

static void
get_approx_canvas_size_px (ChafaCanvasConfig *config, gint *target_width_out,
                           gint *target_height_out)
{
    gint cell_width_px, cell_height_px;

    chafa_canvas_config_get_cell_geometry (config, &cell_width_px, &cell_height_px);
    if (cell_width_px < 1 || cell_height_px < 1)
    {
        cell_width_px = 10;
        cell_height_px = 20;
    }

    chafa_canvas_config_get_geometry (config, target_width_out, target_height_out);
    *target_width_out *= cell_width_px;
    *target_height_out *= cell_height_px;
}

static gboolean
format_item (GridLayout *grid, const gchar *path, GString ***gsa)
{
    MediaLoader *media_loader = NULL;
    ChafaPixelType pixel_type;
    gconstpointer pixels;
    ChafaCanvas *canvas = NULL;
    GError *error = NULL;
    gint src_width, src_height, src_rowstride;
    gboolean success = FALSE;
    gint target_width, target_height;

    get_approx_canvas_size_px (grid->canvas_config, &target_width, &target_height);
    media_loader = media_loader_new (path, target_width, target_height, &error);
    if (!media_loader)
    {
        /* FIXME: Use a placeholder image */
        g_clear_error (&error);
        goto out;
    }

    pixels = media_loader_get_frame_data (media_loader,
                                          &pixel_type,
                                          &src_width,
                                          &src_height,
                                          &src_rowstride);
    if (!pixels)
    {
        /* FIXME: Use a placeholder image */
        goto out;
    }

    canvas = build_canvas (pixel_type, pixels,
                           src_width, src_height, src_rowstride, grid->canvas_config,
                           -1,
                           grid->halign, grid->valign,
                           grid->tuck);
    chafa_canvas_print_rows (canvas, grid->term_info, gsa, NULL);
    success = TRUE;

out:
    if (canvas)
        chafa_canvas_unref (canvas);
    if (media_loader)
        media_loader_destroy (media_loader);
    return success;
}

static gboolean
print_grid_row_symbols (GridLayout *grid, ChafaTerm *term)
{
    GString **item_gsa [MAX_COLS];
    GList *paths = NULL;
    gint item_height [MAX_COLS];
    gint col_width, row_height;
    gint n_cols_produced;
    gint i, j;

    if (grid->finished_chunks)
        return FALSE;

    chafa_canvas_config_get_geometry (grid->canvas_config, &col_width, &row_height);

    for (i = 0; i < MAX_COLS; i++)
        item_height [i] = G_MAXINT;

    for (n_cols_produced = 0; n_cols_produced < grid->n_cols && grid->next_path; )
    {
        if (format_item (grid, grid->next_path->data, &item_gsa [n_cols_produced]))
        {
            paths = g_list_prepend (paths, grid->next_path->data);
            n_cols_produced++;
        }
        else
        {
            /* FIXME: Use a placeholder image */
        }

        grid->next_path = g_list_next (grid->next_path);
        grid->next_item++;
    }

    if (n_cols_produced < 1)
    {
        grid->finished_chunks = TRUE;
        return FALSE;
    }

    paths = g_list_reverse (paths);

    for (i = 0; i < row_height; i++)
    {
        for (j = 0; j < n_cols_produced; j++)
        {
            if (i < item_height [j] && !item_gsa [j] [i])
            {
                /* Pad with spaces */
                item_height [j] = i;
            }
        }
    }

    for (i = 0; i < row_height; i++)
    {
        for (j = 0; j < n_cols_produced; j++)
        {
            if (i >= item_height [j])
            {
                /* Pad with spaces */
                print_rep_char (term, ' ', col_width + 1);
            }
            else
            {
                gint col_row_data_len = item_gsa [j] [i]->len;

                if (j > 0)
                    chafa_term_write (term, " ", 1);

                chafa_term_write (term, item_gsa [j] [i]->str, col_row_data_len);
            }
        }

        chafa_term_write (term, "\n", 1);
    }

    if (grid->print_labels)
    {
        GList *l = paths;

        for (j = 0; l && j < n_cols_produced; j++)
        {
            const gchar *path = l->data;
            path_print_label (term, path, grid->halign, col_width, grid->use_unicode);
            chafa_term_write (term, " ", 1);
            l = g_list_next (l);
        }

        chafa_term_write (term, "\n", 1);
    }

    chafa_term_write (term, "\n", 1);

    for (i = 0; i < n_cols_produced; i++)
        chafa_free_gstring_array (item_gsa [i]);

    g_list_free (paths);
    return TRUE;
}

static gboolean
print_grid_image (GridLayout *grid, ChafaTerm *term)
{
    GString **item_gsa [1] = { NULL };
    const gchar *path = "?";
    gint col_width, row_height;
    gint i, j;

    if (grid->finished_chunks)
        return FALSE;

    /* Setup */

    chafa_canvas_config_get_geometry (grid->canvas_config, &col_width, &row_height);

    /* Format first available item */

    while (grid->next_path && !item_gsa [0])
    {
        if (!format_item (grid, grid->next_path->data, &item_gsa [0]))
        {
            /* FIXME: Use a placeholder image */
        }

        path = grid->next_path->data;
        grid->next_path = g_list_next (grid->next_path);
    }

    /* Optional: End previous row */

    if (grid->next_item != 0 &&
        (grid->next_item % grid->n_cols == 0 || !item_gsa [0]))
    {
        for (i = 0; i < row_height + (grid->print_labels ? 1 : 0) + 1; i++)
        {
            chafa_term_print_seq (term, CHAFA_TERM_SEQ_CURSOR_DOWN_SCROLL, -1);
        }

        /* FIXME: Make relative */
        chafa_term_write (term, "\r", 1);
    }

    if (!item_gsa [0])
    {
        grid->finished_chunks = TRUE;
        return FALSE;
    }

    /* Optional: Begin row */

    if (grid->next_item % grid->n_cols == 0)
    {
        /* Reserve space on terminal, scrolling if necessary */

        for (i = 0; i < row_height + 1; i++)
        {
            chafa_term_print_seq (term, CHAFA_TERM_SEQ_CURSOR_DOWN_SCROLL, -1);
        }
        chafa_term_print_seq (term, CHAFA_TERM_SEQ_CURSOR_UP, row_height + 1, -1);
    }

    /* Format image */

    chafa_term_print_seq (term, CHAFA_TERM_SEQ_SAVE_CURSOR_POS, -1);

    for (j = 0; item_gsa [0] [j]; j++)
    {
        gint col_row_data_len = item_gsa [0] [j]->len;
        chafa_term_write (term, item_gsa [0] [j]->str, col_row_data_len);
    }

    /* Print label */

    if (grid->print_labels)
    {
        chafa_term_print_seq (term, CHAFA_TERM_SEQ_RESTORE_CURSOR_POS, -1);
        chafa_term_print_seq (term, CHAFA_TERM_SEQ_CURSOR_DOWN, row_height, -1);
        path_print_label (term, path, grid->halign, col_width, grid->use_unicode);
    }

    /* Prepare for next image */

    chafa_term_print_seq (term, CHAFA_TERM_SEQ_RESTORE_CURSOR_POS, -1);
    chafa_term_print_seq (term, CHAFA_TERM_SEQ_CURSOR_RIGHT, col_width + 1, -1);

    grid->next_item++;

    chafa_free_gstring_array (item_gsa [0]);
    return TRUE;
}

static gboolean
print_grid_chunk (GridLayout *grid, ChafaTerm *term)
{
    ChafaPixelMode pixel_mode;

    pixel_mode = chafa_canvas_config_get_pixel_mode (grid->canvas_config);

    if (pixel_mode == CHAFA_PIXEL_MODE_SYMBOLS)
        return print_grid_row_symbols (grid, term);
    else
        return print_grid_image (grid, term);
}

GridLayout *
grid_layout_new (void)
{
    GridLayout *grid;

    grid = g_new0 (GridLayout, 1);
    grid->tuck = CHAFA_TUCK_FIT;
    return grid;
}

void
grid_layout_destroy (GridLayout *grid)
{
    if (grid->canvas_config)
        chafa_canvas_config_unref (grid->canvas_config);
    if (grid->term_info)
        chafa_term_info_unref (grid->term_info);
    g_list_free_full (g_steal_pointer (&grid->paths), g_free);
    g_free (grid);
}

void
grid_layout_set_canvas_config (GridLayout *grid, ChafaCanvasConfig *canvas_config)
{
    g_return_if_fail (grid != NULL);

    chafa_canvas_config_ref (canvas_config);
    if (grid->canvas_config)
        chafa_canvas_config_unref (grid->canvas_config);
    grid->canvas_config = canvas_config;

    update_geometry (grid);
}

void
grid_layout_set_term_info (GridLayout *grid, ChafaTermInfo *term_info)
{
    g_return_if_fail (grid != NULL);

    chafa_term_info_ref (term_info);
    if (grid->term_info)
        chafa_term_info_unref (grid->term_info);
    grid->term_info = term_info;

    update_geometry (grid);
}

void
grid_layout_set_view_size (GridLayout *grid, gint width, gint height)
{
    g_return_if_fail (grid != NULL);

    grid->view_width = width;
    grid->view_height = height;

    update_geometry (grid);
}

void
grid_layout_set_grid_size (GridLayout *grid, gint n_cols, gint n_rows)
{
    g_return_if_fail (grid != NULL);

    grid->n_cols = MIN (n_cols, MAX_COLS);
    grid->n_rows = n_rows;

    update_geometry (grid);
}

void
grid_layout_set_align (GridLayout *grid, ChafaAlign halign, ChafaAlign valign)
{
    g_return_if_fail (grid != NULL);

    grid->halign = halign;
    grid->valign = valign;
}

void
grid_layout_set_tuck (GridLayout *grid, ChafaTuck tuck)
{
    g_return_if_fail (grid != NULL);

    grid->tuck = tuck;
}

void
grid_layout_set_print_labels (GridLayout *grid, gboolean print_labels)
{
    g_return_if_fail (grid != NULL);

    grid->print_labels = print_labels;
}

void
grid_layout_set_use_unicode (GridLayout *grid, gboolean use_unicode)
{
    g_return_if_fail (grid != NULL);

    grid->use_unicode = use_unicode;
}

void
grid_layout_push_path (GridLayout *grid, const gchar *path)
{
    g_return_if_fail (grid != NULL);
    g_return_if_fail (path != NULL);
    g_return_if_fail (grid->finished_push == FALSE);

    grid->paths = g_list_prepend (grid->paths, g_strdup (path));
}

gboolean
grid_layout_print_chunk (GridLayout *grid, ChafaTerm *term)
{
    g_return_val_if_fail (grid != NULL, FALSE);

    if (!grid->finished_push)
    {
        grid->paths = g_list_reverse (grid->paths);
        grid->n_items = g_list_length (grid->paths);
        grid->next_path = grid->paths;
        grid->finished_push = TRUE;

        if (!grid->canvas_config)
            grid->canvas_config = chafa_canvas_config_new ();
        if (!grid->term_info)
            grid->term_info = chafa_term_db_get_fallback_info (chafa_term_db_get_default ());

        update_geometry (grid);
    }

    return print_grid_chunk (grid, term);
}
