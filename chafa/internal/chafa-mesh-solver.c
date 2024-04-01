/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2024 Hans Petter Jansson
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

#include <string.h>
#include "internal/chafa-private.h"
#include "internal/chafa-canvas-internal.h"
#include "internal/chafa-mesh-solver.h"
#include "internal/chafa-vector.h"
#include "internal/smolscale/smolscale.h"

#define DEBUG(x) x

#define LOCAL_ITERS 100
#define GLOBAL_ITERS 1000

/* FIXME: May have to scale down first to remove noise in big pictures */

/* Prescale to 16 megapixels */
#define PRESCALE_N_PIXELS (4096 * 4096)

#define CHAFA_SQUARE(a) ((a) * (a))

static gint
sigmoid_ascent (gint i, gint max)
{
    /* Higher values make it more linear, lower values approaching a
     * threshold function */
    const gfloat linearity = .25f;

    return (tanhf ((i - (max / 2)) / (max * linearity)) + 1) * (max / 2);
}

static gint
sigmoid_iter (const ChafaMeshSolver *solver)
{
#if 0
    for (gint i = 0; i < GLOBAL_ITERS; i++)
        g_printerr ("%d -> %d\n", i, sigmoid_ascent (i, GLOBAL_ITERS));
#endif

    return sigmoid_ascent (solver->iterations, GLOBAL_ITERS);
}

static gint64
total_error (const ChafaMeshSolver *solver, const ChafaCellStats *stats)
{
    return stats->shape_error + stats->color_error + (sigmoid_iter (solver) * stats->mesh_error * 1024)
        / (GLOBAL_ITERS * 32);
}

static gint
color_diff_linear (const ChafaColor *a, const ChafaColor *b)
{
    return abs ((gint) a->ch [0] - (gint) b->ch [0])
        + abs ((gint) a->ch [1] - (gint) b->ch [1])
        + abs ((gint) a->ch [2] - (gint) b->ch [2]);
}

static gint64
color_pair_contrast (const ChafaCanvasCell *cell)
{
    ChafaColorPair max_pair = { { { { 0, 0, 0, 0 } }, { { 255, 255, 255, 255 } } } };
    ChafaColorPair pair;
    gint ch_diff [3];
    gint64 diff;
    gint i;

    chafa_unpack_color (cell->fg_color, &pair.colors [0]);
    chafa_unpack_color (cell->bg_color, &pair.colors [1]);

    for (i = 0; i < 3; i++)
        ch_diff [i] = 255 - abs ((gint) pair.colors [0].ch [i] - (gint) pair.colors [1].ch [i]);

    return ch_diff [0] + ch_diff [1] + ch_diff [2];
}

static gint64
color_pair_uncaptured (const ChafaCanvasCell *cell, const ChafaWorkCell *wcell)
{
    ChafaColorPair max_pair = { { { { 0, 0, 0, 0 } }, { { 255, 255, 255, 255 } } } };
    ChafaColorPair pair;
    gint ch_diff [3];
    gint64 diff = 0;
    gint i;

    chafa_unpack_color (cell->fg_color, &pair.colors [0]);
    chafa_unpack_color (cell->bg_color, &pair.colors [1]);

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        gint64 a, b;

#if 0
        a = chafa_color_diff_fast (&pair.colors [0], &wcell->pixels [i].col);
        b = chafa_color_diff_fast (&pair.colors [1], &wcell->pixels [i].col);
#else
        a = color_diff_linear (&pair.colors [0], &wcell->pixels [i].col);
        b = color_diff_linear (&pair.colors [1], &wcell->pixels [i].col);
#endif

        diff += MIN (a, b);
    }

    return diff;
}

static void
calc_prescale_dims (ChafaMeshSolver *solver,
                    gint width, gint height,
                    gint *width_out, gint *height_out)
{
    gdouble factor;

#if 0
    /* Big */
    factor = sqrt ((gdouble) PRESCALE_N_PIXELS / (gdouble) (width * height));
    *width_out = factor * width + 0.5;
    *height_out = factor * height + 0.5;
#elif 1
    /* No scaling */
    factor = 1.0;
    *width_out = width;
    *height_out = height;
#elif 0
    /* According to cell extent */
    *width_out = solver->cells_width * CHAFA_SYMBOL_WIDTH_PIXELS * 2;
    *height_out = solver->cells_height * CHAFA_SYMBOL_HEIGHT_PIXELS * 2;
    factor = *width_out / (gdouble) width;
#endif

    DEBUG (g_printerr ("Input %dx%d -> Factor = %lf\n", width, height, factor));
}

static void
prescale_frame (ChafaMeshSolver *solver, gpointer pixels,
                gint width, gint height, gint rowstride)
{
    calc_prescale_dims (solver,
                        width,
                        height,
                        &solver->pixels_width,
                        &solver->pixels_height);
    solver->pixels_rowstride = solver->pixels_width * 4;

    DEBUG (g_printerr ("Prescale size = %dx%d\n", solver->pixels_width, solver->pixels_height));

    /* FIXME: Use g_try_malloc() */
    solver->pixels = g_malloc (solver->pixels_width
                               * solver->pixels_height * 4);

    smol_scale_simple (pixels,
                       (SmolPixelType) SMOL_PIXEL_BGRA8_UNASSOCIATED,  /* ??? */
                       width,
                       height,
                       rowstride,
                       solver->pixels,
                       SMOL_PIXEL_RGBA8_UNASSOCIATED,
                       solver->pixels_width,
                       solver->pixels_height,
                       solver->pixels_rowstride,
                       SMOL_NO_FLAGS);
}

static ChafaCellIndex *
init_index (gint width, gint height)
{
    ChafaCellIndex *index;
    gint x, y;

    index = g_new (ChafaCellIndex, width * height);

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            ChafaCellIndex *ci = &index [x + y * width];
            ci->x = x;
            ci->y = y;
        }
    }

    return index;
}

static ChafaCellStats *
init_stats (gint width, gint height)
{
    ChafaCellStats *stats;
    gint x, y;

    stats = g_new (ChafaCellStats, width * height);

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            ChafaCellStats *s = &stats [x + y * width];

            memset (s, 0, sizeof (*s));
            s->is_dirty = TRUE;
        }
    }

    return stats;
}

static gint64
sample_cell (ChafaMeshSolver *solver, gint cell_x, gint cell_y,
             ChafaCanvasCell *cell_out, ChafaWorkCell *wcell_out,
             ChafaCellStats *stats_out)
{
    ChafaCellStats stats;

    if (cell_x < 0 || cell_x >= solver->cells_width
        || cell_y < 0 || cell_y >= solver->cells_height)
        return 0;

    if (!stats_out)
    {
        memset (&stats, 0, sizeof (stats));
        stats_out = &stats;
    }

    chafa_work_cell_init_empty (wcell_out);
    chafa_mesh_oversample_cell (solver->mesh, wcell_out, cell_x, cell_y,
                                solver->pixels, solver->pixels_width, solver->pixels_height,
                                solver->pixels_rowstride,
                                .5f);

    stats_out->shape_error = solver->update_func (wcell_out, cell_out, solver->update_func_data);

#if 0
    stats_out->color_error = 3000 * color_pair_contrast (cell_out);  /* FIXME: Should be linear? */
    stats_out->mesh_error =
        300.f * chafa_mesh_get_cell_drift (solver->mesh, cell_x, cell_y)
        + 80.f * chafa_mesh_get_cell_deform (solver->mesh, cell_x, cell_y)
        + 1000000.f * chafa_mesh_get_cell_misalign (solver->mesh, cell_x, cell_y);
#else
/* stats_out->color_error = 100 * color_pair_uncaptured (cell_out, wcell_out) */
    /* + 10 * color_pair_contrast (cell_out) */;

    stats_out->color_error = -(chafa_work_cell_get_local_contrast (wcell_out) / 16);
#if 0
    stats_out->color_error = color_pair_contrast (cell_out);
#endif
    stats_out->mesh_error = .5f * (
/*        1.5f * chafa_mesh_get_cell_drift (solver->mesh, cell_x, cell_y) */
        + .5f * chafa_mesh_get_cell_deform (solver->mesh, cell_x, cell_y)
        + 1200.f * chafa_mesh_get_cell_misalign (solver->mesh, cell_x, cell_y));
#endif

    return total_error (solver, stats_out);
}

static int 
compare_shape_error (const void *av, const void *bv, void *data)
{
    ChafaMeshSolver *solver = data;
    const ChafaCellIndex *a = av, *b = bv;
    const ChafaCellStats *sa = &solver->stats [a->x + a->y * solver->cells_width];
    const ChafaCellStats *sb = &solver->stats [b->x + b->y * solver->cells_width];

    if (sa->shape_error > sb->shape_error)
        return -1;
    else if (sa->shape_error < sb->shape_error)
        return 1;
    return 0;
}

static int 
compare_color_error (const void *av, const void *bv, void *data)
{
    ChafaMeshSolver *solver = data;
    const ChafaCellIndex *a = av, *b = bv;
    const ChafaCellStats *sa = &solver->stats [a->x + a->y * solver->cells_width];
    const ChafaCellStats *sb = &solver->stats [b->x + b->y * solver->cells_width];

    if (sa->color_error > sb->color_error)
        return -1;
    else if (sa->color_error < sb->color_error)
        return 1;
    return 0;
}

static int 
compare_mesh_error (const void *av, const void *bv, void *data)
{
    ChafaMeshSolver *solver = data;
    const ChafaCellIndex *a = av, *b = bv;
    const ChafaCellStats *sa = &solver->stats [a->x + a->y * solver->cells_width];
    const ChafaCellStats *sb = &solver->stats [b->x + b->y * solver->cells_width];

    if (sa->mesh_error > sb->mesh_error)
        return -1;
    else if (sa->mesh_error < sb->mesh_error)
        return 1;
    return 0;
}

static int 
compare_priority (const void *av, const void *bv, void *data)
{
    ChafaMeshSolver *solver = data;
    const ChafaCellIndex *a = av, *b = bv;
    const ChafaCellStats *sa = &solver->stats [a->x + a->y * solver->cells_width];
    const ChafaCellStats *sb = &solver->stats [b->x + b->y * solver->cells_width];
    gint64 ea = total_error (solver, sa)
        + (1LL << (MIN ((solver->iterations - sa->attempt_stamp) / 4, 30)));
    gint64 eb = total_error (solver, sb)
        + (1LL << (MIN ((solver->iterations - sb->attempt_stamp) / 4, 30)));

    if (ea > eb)
        return -1;
    else if (ea < eb)
        return 1;
    return 0;
}

static void
sort_index (ChafaMeshSolver *solver, ChafaCellIndex *index,
            int (*comparator) (const void *, const void *, void *))
{
    qsort_r (index,
             solver->cells_width * solver->cells_height,
             sizeof (ChafaCellIndex),
             comparator,
             solver);
}

static void
refresh_cells (ChafaMeshSolver *solver)
{
    gint x, y;

    for (y = 0; y < solver->cells_height; y++)
    {
        for (x = 0; x < solver->cells_width; x++)
        {
            ChafaCanvasCell *cell = &solver->cells [x + y * solver->cells_rowstride];
            ChafaCellStats *stats = &solver->stats [x + y * solver->cells_width];
            ChafaWorkCell wcell;

            if (!stats->is_dirty)
                continue;

            sample_cell (solver, x, y, cell, &wcell, stats);
            stats->is_dirty = FALSE;
        }
    }

    sort_index (solver, solver->index_by_shape_error, compare_shape_error);
    sort_index (solver, solver->index_by_color_error, compare_color_error);
    sort_index (solver, solver->index_by_mesh_error, compare_mesh_error);
    sort_index (solver, solver->index_by_priority, compare_priority);
}

void
chafa_mesh_solver_init (ChafaMeshSolver *solver,
                        gpointer pixels,
                        gint pixels_width, gint pixels_height,
                        gint pixels_rowstride,
                        ChafaCanvasCell *cells,
                        gint cells_width, gint cells_height,
                        gint cells_rowstride,
                        ChafaUpdateCellFunc *update_func,
                        gpointer update_func_data)
{
    g_return_if_fail (solver != NULL);
    g_return_if_fail (pixels != NULL);
    g_return_if_fail (pixels_width > 0);
    g_return_if_fail (pixels_height > 0);
    g_return_if_fail (cells_width > 0);
    g_return_if_fail (cells_height > 0);
    g_return_if_fail (update_func != NULL);

    solver->cells = cells;
    solver->cells_width = cells_width;
    solver->cells_height = cells_height;
    solver->cells_rowstride = cells_rowstride;
    solver->update_func = update_func;
    solver->update_func_data = update_func_data;

    prescale_frame (solver, pixels, pixels_width, pixels_height, pixels_rowstride);

    solver->stats = init_stats (cells_width, cells_height);
    solver->index_by_shape_error = init_index (cells_width, cells_height);
    solver->index_by_color_error = init_index (cells_width, cells_height);
    solver->index_by_mesh_error = init_index (cells_width, cells_height);
    solver->index_by_priority = init_index (cells_width, cells_height);
    solver->mesh = chafa_mesh_new (solver->pixels_width, solver->pixels_height,
                                   solver->cells_width, solver->cells_height);
}

void
chafa_mesh_solver_deinit (ChafaMeshSolver *solver)
{
    g_return_if_fail (solver != NULL);

    g_free (solver->pixels);
    g_free (solver->stats);
    g_free (solver->index_by_shape_error);
    g_free (solver->index_by_color_error);
    g_free (solver->index_by_mesh_error);
}

ChafaMeshSolver *
chafa_mesh_solver_new (gpointer pixels,
                       gint pixels_width, gint pixels_height,
                       gint pixels_rowstride,
                       ChafaCanvasCell *cells,
                       gint cells_width, gint cells_height,
                       gint cells_rowstride,
                       ChafaUpdateCellFunc *update_func,
                       gpointer update_func_data)
{
    ChafaMeshSolver *solver;

    g_return_val_if_fail (pixels != NULL, NULL);
    g_return_val_if_fail (pixels_width > 0, NULL);
    g_return_val_if_fail (pixels_height > 0, NULL);
    g_return_val_if_fail (cells != NULL, NULL);
    g_return_val_if_fail (cells_width > 0, NULL);
    g_return_val_if_fail (cells_height > 0, NULL);
    g_return_val_if_fail (update_func != NULL, NULL);

    solver = g_new0 (ChafaMeshSolver, 1);

    chafa_mesh_solver_init (solver, pixels, pixels_width, pixels_height, pixels_rowstride,
                            cells, cells_width, cells_height, cells_rowstride,
                            update_func, update_func_data);
    return solver;
}

static gint64
update_mesh_cell (ChafaMeshSolver *solver,
                  gint point_x, gint point_y,
                  gint cell_ofs_x, gint cell_ofs_y)
{
    ChafaWorkCell wcell;
    ChafaCanvasCell *cell;
    ChafaCellStats *stats;
    gint x, y;

    x = point_x + cell_ofs_x;
    y = point_y + cell_ofs_y;
    cell = &solver->cells [x + y * solver->cells_rowstride];
    stats = &solver->stats [x + y * solver->cells_width];

    return sample_cell (solver, x, y, cell, &wcell, stats);
}

static gint64
update_cell_rect (ChafaMeshSolver *solver,
                  gint x, gint y,
                  gint width, gint height)
{
    gint i, j;
    gint64 badness = 0;

    for (i = y; i < y + height; i++)
    {
        for (j = x; j < x + width; j++)
        {
            badness += update_mesh_cell (solver, j, i, 0, 0);
        }
    }

    return badness;
}

static gint
update_mesh_3x3 (ChafaMeshSolver *solver, gint point_x, gint point_y)
{
    return ((update_mesh_cell (solver, point_x, point_y, -1, -1)
            + update_mesh_cell (solver, point_x, point_y,   0, -1)
            + update_mesh_cell (solver, point_x, point_y,   1, -1)
            + update_mesh_cell (solver, point_x, point_y,  -1,  0)
            + update_mesh_cell (solver, point_x, point_y,   1,  0)
            + update_mesh_cell (solver, point_x, point_y,  -1,  1)
            + update_mesh_cell (solver, point_x, point_y,   0,  1)
            + update_mesh_cell (solver, point_x, point_y,   1,  1)) / 8)
        + update_mesh_cell (solver, point_x, point_y,   0,  0);
}

static void
optimize_3x3 (ChafaMeshSolver *solver, gint x, gint y)
{
    gfloat best_error = G_MAXFLOAT;
    gint i;

    if (x < 1 || x >= solver->cells_width - 1
        || y < 1 || y >= solver->cells_height - 1)
        return;

    for (i = 0; i < LOCAL_ITERS; i++)
    {
        gfloat rel_def = 100.f * (chafa_mesh_get_relative_deformity (solver->mesh, x, y)
                                  + chafa_mesh_get_relative_deformity (solver->mesh, x + 1, y)
                                  + chafa_mesh_get_relative_deformity (solver->mesh, x, y + 1)
                                  + chafa_mesh_get_relative_deformity (solver->mesh, x + 1, y + 1));
#if 0
        gfloat abs_def = 200000.f * (chafa_mesh_get_absolute_deformity (solver->mesh, x, y)
                                     + chafa_mesh_get_absolute_deformity (solver->mesh, x + 1, y)
                                     + chafa_mesh_get_absolute_deformity (solver->mesh, x, y + 1)
                                     + chafa_mesh_get_absolute_deformity (solver->mesh, x + 1, y + 1));
#else
        gfloat abs_def = 0.0f;
#endif
#if 0
        gint sym_err = update_mesh_cell (solver, x, y, 0, 0);
#else
        gint sym_err = update_mesh_3x3 (solver, x, y);
#endif
        gfloat error = sym_err + abs_def + rel_def;

#if 0
        g_print ("sym=%8d  abs=%8d  rel=%8d\n",
                 sym_err,
                 (gint) abs_def,
                 (gint) rel_def);
#endif

        if (error < best_error)
        {
            chafa_mesh_save_point (solver->mesh, x, y);
            chafa_mesh_save_point (solver->mesh, x + 1, y);
            chafa_mesh_save_point (solver->mesh, x, y + 1);
            chafa_mesh_save_point (solver->mesh, x + 1, y + 1);
            best_error = error;
        }
        else
        {
            chafa_mesh_restore_point (solver->mesh, x, y);
            chafa_mesh_restore_point (solver->mesh, x + 1, y);
            chafa_mesh_restore_point (solver->mesh, x, y + 1);
            chafa_mesh_restore_point (solver->mesh, x + 1, y + 1);
        }

        chafa_mesh_perturb_point (solver->mesh, x, y, 0.25f);
        chafa_mesh_perturb_point (solver->mesh, x + 1, y, 0.25f);
        chafa_mesh_perturb_point (solver->mesh, x, y + 1, 0.25f);
        chafa_mesh_perturb_point (solver->mesh, x + 1, y + 1, 0.25f);
    }

    chafa_mesh_restore_point (solver->mesh, x, y);
    chafa_mesh_restore_point (solver->mesh, x + 1, y);
    chafa_mesh_restore_point (solver->mesh, x, y + 1);
    chafa_mesh_restore_point (solver->mesh, x + 1, y + 1);
    update_mesh_3x3 (solver, x, y);
}

static gint
update_mesh_point (ChafaMeshSolver *solver,
                   gint point_x, gint point_y)
{
    return update_mesh_cell (solver, point_x, point_y, -1, -1)
        + update_mesh_cell (solver, point_x, point_y, 0, -1)
        + update_mesh_cell (solver, point_x, point_y, -1, 0)
        + update_mesh_cell (solver, point_x, point_y, 0, 0);
}

static void
optimize_point (ChafaMeshSolver *solver, gint x, gint y)
{
    gint i;
    gint64 best_error = G_MAXINT64;

    for (i = 0; i < LOCAL_ITERS; i++)
    {
        gfloat rel_def = 100.f * chafa_mesh_get_relative_deformity (solver->mesh, x, y);
#if 0
        gfloat abs_def = 100000.f * chafa_mesh_get_absolute_deformity (solver->mesh, x, y);
#else
        gfloat abs_def = 0.0f;
#endif
        gint64 sym_err = update_mesh_point (solver, x, y);
        gint64 error = sym_err + abs_def + rel_def;

#if 0
        g_print ("sym=%8d  abs=%8d  rel=%8d\n",
                 sym_err,
                 (gint) abs_def,
                 (gint) rel_def);
#endif

        if (error < best_error)
        {
            chafa_mesh_save_point (solver->mesh, x, y);
            best_error = error;
        }
        else
        {
            chafa_mesh_restore_point (solver->mesh, x, y);
        }

        chafa_mesh_perturb_point (solver->mesh, x, y, 1.0f);
    }

    chafa_mesh_restore_point (solver->mesh, x, y);
    update_mesh_point (solver, x, y);
}

static void
optimize_global_3x3 (ChafaMeshSolver *solver)
{
    gint x, y;

    for (y = 0; y < solver->cells_height + 1; y += 3)
        for (x = 0; x < solver->cells_width + 1; x += 3)
            optimize_3x3 (solver, x, y);

    for (y = 1; y < solver->cells_height + 1; y += 3)
        for (x = 1; x < solver->cells_width + 1; x += 3)
            optimize_3x3 (solver, x, y);

    for (y = 2; y < solver->cells_height + 1; y += 3)
        for (x = 2; x < solver->cells_width + 1; x += 3)
            optimize_3x3 (solver, x, y);

    for (y = 0; y < solver->cells_height + 1; y += 3)
        for (x = 1; x < solver->cells_width + 1; x += 3)
            optimize_3x3 (solver, x, y);

    for (y = 1; y < solver->cells_height + 1; y += 3)
        for (x = 2; x < solver->cells_width + 1; x += 3)
            optimize_3x3 (solver, x, y);

    for (y = 2; y < solver->cells_height + 1; y += 3)
        for (x = 0; x < solver->cells_width + 1; x += 3)
            optimize_3x3 (solver, x, y);

    for (y = 0; y < solver->cells_height + 1; y += 3)
        for (x = 2; x < solver->cells_width + 1; x += 3)
            optimize_3x3 (solver, x, y);

    for (y = 1; y < solver->cells_height + 1; y += 3)
        for (x = 0; x < solver->cells_width + 1; x += 3)
            optimize_3x3 (solver, x, y);

    for (y = 2; y < solver->cells_height + 1; y += 3)
        for (x = 1; x < solver->cells_width + 1; x += 3)
            optimize_3x3 (solver, x, y);

    for (y = 0; y < solver->cells_height + 1; y += 2)
    {
        for (x = 0; x < solver->cells_width + 1; x += 2)
        {
            optimize_point (solver, x, y);
        }
    }

    for (y = 1; y < solver->cells_height + 1; y += 2)
    {
        for (x = 1; x < solver->cells_width + 1; x += 2)
        {
            optimize_point (solver, x, y);
        }
    }

    for (y = 0; y < solver->cells_height + 1; y += 2)
    {
        for (x = 1; x < solver->cells_width + 1; x += 2)
        {
            optimize_point (solver, x, y);
        }
    }

    for (y = 1; y < solver->cells_height + 1; y += 2)
    {
        for (x = 0; x < solver->cells_width + 1; x += 2)
        {
            optimize_point (solver, x, y);
        }
    }
}

static void
optimize_global_point (ChafaMeshSolver *solver)
{
    gint x, y;

    for (y = 0; y < solver->cells_height + 1; y += 2)
    {
        for (x = 0; x < solver->cells_width + 1; x += 2)
        {
            optimize_point (solver, x, y);
        }
    }

    for (y = 1; y < solver->cells_height + 1; y += 2)
    {
        for (x = 1; x < solver->cells_width + 1; x += 2)
        {
            optimize_point (solver, x, y);
        }
    }

    for (y = 0; y < solver->cells_height + 1; y += 2)
    {
        for (x = 1; x < solver->cells_width + 1; x += 2)
        {
            optimize_point (solver, x, y);
        }
    }

    for (y = 1; y < solver->cells_height + 1; y += 2)
    {
        for (x = 0; x < solver->cells_width + 1; x += 2)
        {
            optimize_point (solver, x, y);
        }
    }
}

static void
optimize_global_translations (ChafaMeshSolver *solver)
{
    gfloat x, y;
    gfloat best_x, best_y;
    gint64 best_badness;

    best_x = .0f;
    best_y = .0f;
    best_badness = update_cell_rect (solver, 0, 0,
                                     solver->cells_width,
                                     solver->cells_height);

    for (x = -0.5f; x < 0.51f; x += 0.005f)
    {
        for (y = -0.5f; y < 0.51f; y += 0.005f)
        {
            gint64 badness;

            chafa_mesh_translate_rect (solver->mesh, 1, 1,
                                       solver->cells_width - 1,
                                       solver->cells_height - 1,
                                       x, y);
            badness = update_cell_rect (solver, 0, 0,
                                        solver->cells_width,
                                        solver->cells_height);
            if (badness < best_badness)
            {
                best_x = x;
                best_y = y;
                best_badness = badness;
            }

            chafa_mesh_restore_all (solver->mesh);
        }
    }

    g_printerr ("best_x=%f, best_y=%f\n", best_x, best_y);

    chafa_mesh_translate_rect (solver->mesh, 1, 1,
                               solver->cells_width - 1,
                               solver->cells_height - 1,
                               best_x, best_y);
    chafa_mesh_save_all (solver->mesh);
    update_cell_rect (solver, 0, 0,
                      solver->cells_width,
                      solver->cells_height);
}

static void
optimize_global_scale (ChafaMeshSolver *solver)
{
    gfloat x, y;
    gfloat best_x, best_y;
    gint64 best_badness;

    best_x = .0f;
    best_y = .0f;
    best_badness = update_cell_rect (solver, 0, 0,
                                     solver->cells_width,
                                     solver->cells_height);

    for (x = -0.4f; x < 0.41f; x += 0.01f)
    {
        for (y = -0.4f; y < 0.41f; y += 0.01f)
        {
            gint64 badness;

            chafa_mesh_scale_rect (solver->mesh, 1, 1,
                                   solver->cells_width - 1,
                                   solver->cells_height - 1,
                                   x, y);
            badness = update_cell_rect (solver, 0, 0,
                                        solver->cells_width,
                                        solver->cells_height);
            if (badness < best_badness)
            {
                best_x = x;
                best_y = y;
                best_badness = badness;
            }

            chafa_mesh_restore_all (solver->mesh);
        }
    }

    g_printerr ("best_x=%f, best_y=%f\n", best_x, best_y);

    chafa_mesh_scale_rect (solver->mesh, 1, 1,
                               solver->cells_width - 1,
                               solver->cells_height - 1,
                               best_x, best_y);
    chafa_mesh_save_all (solver->mesh);
    update_cell_rect (solver, 0, 0,
                      solver->cells_width,
                      solver->cells_height);
}

static ChafaCellStats *
stats_from_index (ChafaMeshSolver *solver, ChafaCellIndex *index)
{
    return &solver->stats [index->x + index->y * solver->cells_width];
}

static void
print_stats (ChafaMeshSolver *solver)
{
    gint n = solver->cells_width * solver->cells_height;

    g_printerr ("%4di Color: %ld/%ld/%ld Shape: %ld/%ld/%ld Mesh: %ld/%ld/%ld\n",
                solver->iterations,
                stats_from_index (solver, &solver->index_by_color_error [n - 1])->color_error,
                stats_from_index (solver, &solver->index_by_color_error [n / 2])->color_error,
                stats_from_index (solver, &solver->index_by_color_error [0])->color_error,
                stats_from_index (solver, &solver->index_by_shape_error [n - 1])->shape_error,
                stats_from_index (solver, &solver->index_by_shape_error [n / 2])->shape_error,
                stats_from_index (solver, &solver->index_by_shape_error [0])->shape_error,
                stats_from_index (solver, &solver->index_by_mesh_error [n - 1])->mesh_error,
                stats_from_index (solver, &solver->index_by_mesh_error [n / 2])->mesh_error,
                stats_from_index (solver, &solver->index_by_mesh_error [0])->mesh_error);
}

static void
dirty_rect (ChafaMeshSolver *solver, gint x, gint y, gint width, gint height)
{
    gint u, v;

    for (v = y; v < y + height; v++)
    {
        if (v < 0 || v >= solver->cells_height)
            continue;

        for (u = x; u < x + width; u++)
        {
            ChafaCellStats *stats;

            if (u < 0 || y >= solver->cells_width)
                continue;

            stats = &solver->stats [u + v * solver->cells_width];
            stats->is_dirty = TRUE;
        }
    }
}

typedef struct
{
    ChafaCellStats stats [3] [3];
}
Stats3x3;

static gint64
sample_3x3 (ChafaMeshSolver *solver, gint x, gint y, Stats3x3 *stats3x3_out)
{
    gint64 sum_error = 0;
    gint u, v;

    memset (stats3x3_out, 0, sizeof (*stats3x3_out));

    for (v = MAX (y - 1, 0); v <= MIN (y + 1, solver->cells_height - 1); v++)
    {
        for (u = MAX (x - 1, 0); u <= MIN (x + 1, solver->cells_width - 1); u++)
        {
            ChafaCanvasCell *cell = &solver->cells [u + v * solver->cells_rowstride];
            ChafaWorkCell wcell;

            sum_error += sample_cell (solver, u, v, cell, &wcell,
                &stats3x3_out->stats [v - y + 1] [u - x + 1]);
        }
    }

    return sum_error;
}

static gint64
fetch_3x3 (ChafaMeshSolver *solver, gint x, gint y, Stats3x3 *stats3x3_out)
{
    gint64 sum_error = 0;
    gint u, v;

    for (v = MAX (y - 1, 0); v <= MIN (y + 1, solver->cells_height - 1); v++)
    {
        for (u = MAX (x - 1, 0); u <= MIN (x + 1, solver->cells_width - 1); u++)
        {
            ChafaCanvasCell *cell = &solver->cells [u + v * solver->cells_rowstride];
            ChafaCellStats *stats = &solver->stats [u + v * solver->cells_width];
            ChafaCellStats *stats_out = &stats3x3_out->stats [v - y + 1] [u - x + 1];

            stats_out->shape_error = stats->shape_error;
            stats_out->color_error = stats->color_error;
            stats_out->mesh_error = stats->mesh_error;

            sum_error += total_error (solver, stats);
        }
    }

    return sum_error;
}

static void
commit_3x3 (ChafaMeshSolver *solver, gint x, gint y, const Stats3x3 *stats3x3)
{
    gint u, v;

    for (v = MAX (y - 1, 0); v <= MIN (y + 1, solver->cells_height - 1); v++)
    {
        for (u = MAX (x - 1, 0); u <= MIN (x + 1, solver->cells_width - 1); u++)
        {
            ChafaCanvasCell *cell = &solver->cells [u + v * solver->cells_rowstride];
            ChafaCellStats *stats = &solver->stats [u + v * solver->cells_width];
            const ChafaCellStats *stats_new = &stats3x3->stats [v - y + 1] [u - x + 1];

            stats->shape_error = stats_new->shape_error;
            stats->color_error = stats_new->color_error;
            stats->mesh_error = stats_new->mesh_error;
        }
    }
}

static void
optimize_cell (ChafaMeshSolver *solver, gint x, gint y)
{
    ChafaCanvasCell *cell = &solver->cells [x + y * solver->cells_rowstride];
    ChafaCellStats *stats = &solver->stats [x + y * solver->cells_width];
    gint64 shape_error = stats->shape_error;
    gint64 best_error;
    Stats3x3 stats3x3;
    gboolean dirty = FALSE;
    gint i;

    best_error = fetch_3x3 (solver, x, y, &stats3x3);

    for (i = 0; i < LOCAL_ITERS; i++)
    {
        ChafaWorkCell wcell;
        gfloat rel_def;
        gint64 error;

        chafa_mesh_perturb_point (solver->mesh, x, y, 1.0f / 15.0f);
        chafa_mesh_perturb_point (solver->mesh, x + 1, y, 1.0f / 15.0f);
        chafa_mesh_perturb_point (solver->mesh, x, y + 1, 1.0f / 15.0f);
        chafa_mesh_perturb_point (solver->mesh, x + 1, y + 1, 1.0f / 15.0f);

        error = sample_3x3 (solver, x, y, &stats3x3);

#if 0
        g_printerr ("rel=%.1f, total=%ld, best=%ld\n",
                    rel_def, error, best_error);
#endif

        if (error < best_error)
        {
            chafa_mesh_save_point (solver->mesh, x, y);
            chafa_mesh_save_point (solver->mesh, x + 1, y);
            chafa_mesh_save_point (solver->mesh, x, y + 1);
            chafa_mesh_save_point (solver->mesh, x + 1, y + 1);
            best_error = error;

            commit_3x3 (solver, x, y, &stats3x3);
            dirty = TRUE;
            i = 0;

#if 0
            g_printerr ("Improvement!\n");
#endif
        }
        else
        {
            chafa_mesh_restore_point (solver->mesh, x, y);
            chafa_mesh_restore_point (solver->mesh, x + 1, y);
            chafa_mesh_restore_point (solver->mesh, x, y + 1);
            chafa_mesh_restore_point (solver->mesh, x + 1, y + 1);
        }
    }

    chafa_mesh_restore_point (solver->mesh, x, y);
    chafa_mesh_restore_point (solver->mesh, x + 1, y);
    chafa_mesh_restore_point (solver->mesh, x, y + 1);
    chafa_mesh_restore_point (solver->mesh, x + 1, y + 1);

    stats->attempt_stamp = solver->iterations;
}

static void
optimize_pass (ChafaMeshSolver *solver)
{
    gint i;

    for (i = 0; i < MIN (solver->cells_width * solver->cells_height, 256); i++)
    {
        ChafaCellIndex *index = &solver->index_by_priority [i];

        optimize_cell (solver, index->x, index->y);
    }
}

void
chafa_mesh_solver_solve (ChafaMeshSolver *solver)
{
    gint i;

    for (i = 0; i < GLOBAL_ITERS; i++)
    {
        refresh_cells (solver);
        print_stats (solver);
        optimize_pass (solver);
        solver->iterations++;
    }

#if 0
    for (i = 0; i < GLOBAL_ITERS; i++)
    {
        optimize_global_3x3 (solver);
    }
#endif

#if 0
    optimize_global_scale (solver);
    optimize_global_translations (solver);
#endif

#if 0
    for (i = 0; i < GLOBAL_ITERS; i++)
    {
        optimize_global_point (solver);
    }
#endif

    update_cell_rect (solver, 0, 0,
                      solver->cells_width,
                      solver->cells_height);
}
