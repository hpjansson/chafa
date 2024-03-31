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

#include <string.h>  /* memcpy, memset */
#include "internal/chafa-private.h"
#include "internal/chafa-mesh.h"
#include "internal/chafa-vector.h"
#include "internal/smolscale/smolscale.h"

#define DEBUG(x)

/* --- *
 * API *
 * --- */

typedef struct
{
    ChafaVec2f32 vecs [4];
}
Quad;

static void
cell_corners (ChafaMesh *mesh, gint x, gint y, Quad *quad_out)
{
    quad_out->vecs [0] = mesh->grid [x + y * mesh->grid_width];
    quad_out->vecs [1] = mesh->grid [x + 1 + y * mesh->grid_width];
    quad_out->vecs [2] = mesh->grid [x + (y + 1) * mesh->grid_width];
    quad_out->vecs [3] = mesh->grid [x + 1 + (y + 1) * mesh->grid_width];
}

static void
quad_to_grid_units (Quad *quad, const ChafaVec2f32 *scale)
{
    chafa_vec2f32_hadamard (&quad->vecs [0], &quad->vecs [0], scale);
    chafa_vec2f32_hadamard (&quad->vecs [1], &quad->vecs [1], scale);
    chafa_vec2f32_hadamard (&quad->vecs [2], &quad->vecs [2], scale);
    chafa_vec2f32_hadamard (&quad->vecs [3], &quad->vecs [3], scale);
}

static void
quad_midpoint (ChafaVec2f32 *out, const Quad *corners)
{
    chafa_vec2f32_average_array (out, corners->vecs, 4);
}

static void
quad_extremes (gfloat *min_out, gfloat *max_out,
               const ChafaVec2f32 *midpoint, const Quad *corners)
{
    gfloat dist [4];
    gint i;

    DEBUG (g_printerr ("corners=(%.2f,%.2f)(%.2f,%.2f)(%.2f,%.2f)(%.2f,%.2f), mid=(%f,%f)\n",
                       corners->vecs [0].v [0],
                       corners->vecs [0].v [1],
                       corners->vecs [1].v [0],
                       corners->vecs [1].v [1],
                       corners->vecs [2].v [0],
                       corners->vecs [2].v [1],
                       corners->vecs [3].v [0],
                       corners->vecs [3].v [1],
                       midpoint->v [0],
                       midpoint->v [1]));

    for (i = 0; i < 4; i++)
    {
        ChafaVec2f32 v;

        chafa_vec2f32_sub (&v, &corners->vecs [i], midpoint);
        dist [i] = v.v [0] * v.v [0] + v.v [1] * v.v [1];
    }

    *min_out = sqrtf (MIN (MIN (dist [0], dist [1]), MIN (dist [2], dist [3])));
    *max_out = sqrtf (MAX (MAX (dist [0], dist [1]), MAX (dist [2], dist [3])));
}

static void
init_grid (ChafaMesh *mesh, gint width_cells, gint height_cells)
{
    gint x, y;

    mesh->grid_width = width_cells + 1;
    mesh->grid_height = height_cells + 1;
    mesh->grid = g_new (ChafaVec2f32,
                        mesh->grid_width * mesh->grid_height);
    mesh->saved_grid = g_new (ChafaVec2f32,
                              mesh->grid_width * mesh->grid_height);

    for (y = 0; y < mesh->grid_height; y++)
    {
        for (x = 0; x < mesh->grid_width; x++)
        {
            ChafaVec2f32 *vec = &mesh->grid [x + y * mesh->grid_width];
            vec->v [0] = ((gfloat) x * mesh->width_pixels) / (gfloat) (mesh->grid_width - 1);
            vec->v [1] = ((gfloat) y * mesh->height_pixels) / (gfloat) (mesh->grid_height - 1);
        }
    }

    memcpy (mesh->saved_grid, mesh->grid,
            mesh->grid_width * mesh->grid_height * sizeof (ChafaVec2f32));

    mesh->cell_scale.v [0] = width_cells / (gfloat) mesh->width_pixels;
    mesh->cell_scale.v [1] = height_cells / (gfloat) mesh->height_pixels;

    mesh->cell_scale_inv.v [0] = (gfloat) mesh->width_pixels / width_cells;
    mesh->cell_scale_inv.v [1] = (gfloat) mesh->height_pixels / height_cells;
}

void
chafa_mesh_init (ChafaMesh *mesh,
                 gint width_pixels, gint height_pixels,
                 gint width_cells, gint height_cells)
{
    g_return_if_fail (mesh != NULL);
    g_return_if_fail (width_pixels > 0);
    g_return_if_fail (height_pixels > 0);
    g_return_if_fail (width_cells > 0);
    g_return_if_fail (height_cells > 0);

    mesh->width_pixels = width_pixels;
    mesh->height_pixels = height_pixels;

    init_grid (mesh,
               width_cells, height_cells);
}

void
chafa_mesh_deinit (ChafaMesh *mesh)
{
    g_return_if_fail (mesh != NULL);

    g_free (mesh->grid);
    g_free (mesh->saved_grid);
}

ChafaMesh *
chafa_mesh_new (gint width_pixels, gint height_pixels,
                gint width_cells, gint height_cells)
{
    ChafaMesh *mesh;

    g_return_val_if_fail (width_pixels > 0, NULL);
    g_return_val_if_fail (height_pixels > 0, NULL);
    g_return_val_if_fail (width_cells > 0, NULL);
    g_return_val_if_fail (height_cells > 0, NULL);

    mesh = g_new0 (ChafaMesh, 1);
    chafa_mesh_init (mesh,
                     width_pixels, height_pixels,
                     width_cells, height_cells);
    return mesh;
}

static ChafaVec2f32 *
get_point (ChafaMesh *mesh, gint x, gint y)
{
    return &mesh->grid [x + y * mesh->grid_width];
}

static void
set_point (ChafaMesh *mesh, gint x, gint y, const ChafaVec2f32 *p)
{
    mesh->grid [x + y * mesh->grid_width] = *p;
}

static gfloat
random_step (gfloat p, gfloat min, gfloat max, gfloat step_factor)
{
    gint dir = g_random_int ();

    if (dir < 0)
    {
        p = g_random_double_range (min, p - (p - min) * step_factor);
    }
    else
    {
        p = g_random_double_range (p + (max - p) * step_factor, max);
    }

    return p;
}

void
chafa_mesh_save_point (ChafaMesh *mesh, gint x, gint y)
{
    mesh->saved_grid [x + y * mesh->grid_width]
        = mesh->grid [x + y * mesh->grid_width];
}

void
chafa_mesh_save_all (ChafaMesh *mesh)
{
    memcpy (mesh->saved_grid, mesh->grid,
            mesh->grid_width * mesh->grid_height * sizeof (ChafaVec2f32));
}

void
chafa_mesh_restore_point (ChafaMesh *mesh, gint x, gint y)
{
    mesh->grid [x + y * mesh->grid_width]
        = mesh->saved_grid [x + y * mesh->grid_width];
}

void
chafa_mesh_restore_all (ChafaMesh *mesh)
{
    memcpy (mesh->grid, mesh->saved_grid,
            mesh->grid_width * mesh->grid_height * sizeof (ChafaVec2f32));
}

void
chafa_mesh_translate_rect (ChafaMesh *mesh, gint x, gint y,
                           gint width, gint height,
                           gfloat x_step, gfloat y_step)
{
    ChafaVec2f32 ofs;
    gint i, j;

    ofs.v [0] = x_step;
    ofs.v [1] = y_step;
    chafa_vec2f32_hadamard (&ofs, &ofs, &mesh->cell_scale_inv);

    for (i = y; i < y + height; i++)
    {
        for (j = x; j < x + width; j++)
        {
            ChafaVec2f32 *v = &mesh->grid [j + i * mesh->grid_width];

            chafa_vec2f32_add (v, v, &ofs);
        }
    }
}

void
chafa_mesh_scale_rect (ChafaMesh *mesh, gint x, gint y,
                       gint width, gint height,
                       gfloat x_step, gfloat y_step)
{
    Quad rect;
    ChafaVec2f32 mid;
    ChafaVec2f32 scale;
    gint i, j;

    rect.vecs [0] = mesh->grid [x + y * mesh->grid_width];
    rect.vecs [1] = mesh->grid [x + width + y * mesh->grid_width];
    rect.vecs [2] = mesh->grid [x + (y + height) * mesh->grid_width];
    rect.vecs [3] = mesh->grid [x + width + (y + height) * mesh->grid_width];
    quad_midpoint (&mid, &rect);

    scale.v [0] = (width + x_step) / (gfloat) width;
    scale.v [1] = (height + y_step) / (gfloat) height;

    for (i = y; i < y + height; i++)
    {
        for (j = x; j < x + width; j++)
        {
            ChafaVec2f32 *v = &mesh->grid [j + i * mesh->grid_width];
            ChafaVec2f32 u;

            chafa_vec2f32_sub (&u, v, &mid);
            chafa_vec2f32_hadamard (&u, &u, &scale);
            chafa_vec2f32_add (v, &u, &mid);
        }
    }
}

gboolean
chafa_mesh_nudge_point (ChafaMesh *mesh, gint x, gint y,
                        gfloat x_step, gfloat y_step)
{
    ChafaVec2f32 p;

    if ((x <= 0 || x >= mesh->grid_width - 1)
        && (y <= 0 || y >= mesh->grid_width - 1))
    {
        /* Corner points are fixed in place */
        return FALSE;
    }

    p = *get_point (mesh, x, y);

    if (x > 0 && x < mesh->grid_width - 1)
    {
        ChafaVec2f32 *q;

        if (x_step >= 0.0f)
            q = get_point (mesh, x + 1, y);
        else
            q = get_point (mesh, x - 1, y);

        p.v [0] += (q->v [0] - p.v [0]) * x_step;
    }

    if (y > 0 && y < mesh->grid_height - 1)
    {
        ChafaVec2f32 *q;

        if (y_step >= 0.0f)
            q = get_point (mesh, x, y + 1);
        else
            q = get_point (mesh, x, y - 1);

        p.v [1] += (q->v [1] - p.v [1]) * y_step;
    }

    set_point (mesh, x, y, &p);
    return TRUE;
}

gboolean
chafa_mesh_perturb_point (ChafaMesh *mesh, gint x, gint y, gfloat step_factor)
{
    ChafaVec2f32 p;

    if ((x <= 0 || x >= mesh->grid_width - 1)
        && (y <= 0 || y >= mesh->grid_width - 1))
    {
        /* Corner points are fixed in place */
        return FALSE;
    }

    p = *get_point (mesh, x, y);

    if (x > 0 && x < mesh->grid_width - 1)
    {
        p.v [0] = random_step (p.v [0],
                               get_point (mesh, x - 1, y)->v [0],
                               get_point (mesh, x + 1, y)->v [0],
                               step_factor);
    }

    if (y > 0 && y < mesh->grid_height - 1)
    {
        p.v [1] = random_step (p.v [1],
                               get_point (mesh, x, y - 1)->v [1],
                               get_point (mesh, x, y + 1)->v [1],
                               step_factor);
    }

    set_point (mesh, x, y, &p);
    return TRUE;
}

gfloat
chafa_mesh_get_absolute_deformity (ChafaMesh *mesh,
                                   gint x, gint y)
{
    ChafaVec2f32 *p, q, v;
    gfloat value = 0.0f;

    p = get_point (mesh, x, y);

    q.v [0] = ((gfloat) x * mesh->width_pixels) / (gfloat) (mesh->grid_width - 1);
    q.v [1] = ((gfloat) y * mesh->height_pixels) / (gfloat) (mesh->grid_height - 1);

    chafa_vec2f32_sub (&v, &q, p);
    chafa_vec2f32_hadamard (&v, &v, &mesh->cell_scale);

    return v.v [0] * v.v [0] + v.v [1] * v.v [1];
}

static gfloat
squared_inverse_distance (ChafaMesh *mesh,
                          const ChafaVec2f32 *p, gint x, gint y)
{
    ChafaVec2f32 *q, v;

    if (x < 0 || x >= mesh->grid_width - 1
        || y < 0 || y >= mesh->grid_height - 1)
        return 0.0f;

    q = get_point (mesh, x, y);

    chafa_vec2f32_sub (&v, q, p);
    chafa_vec2f32_hadamard (&v, &v, &mesh->cell_scale);

#if 0
    g_printerr ("v=(%.2f,%.2f)\n", v.v [0], v.v [1]);
#endif

    return 1.f / sqrtf (v.v [0] * v.v [0] + v.v [1] * v.v [1]);
}

static gfloat
squared_distance (ChafaMesh *mesh,
                  const ChafaVec2f32 *p, gint x, gint y)
{
    ChafaVec2f32 *q, v;

    if (x < 0 || x >= mesh->grid_width - 1
        || y < 0 || y >= mesh->grid_height - 1)
        return 0.0f;

    q = get_point (mesh, x, y);

    chafa_vec2f32_sub (&v, q, p);
    chafa_vec2f32_hadamard (&v, &v, &mesh->cell_scale);

#if 0
    g_printerr ("v=(%.2f,%.2f)\n", v.v [0], v.v [1]);
#endif

    return v.v [0] * v.v [0] + v.v [1] * v.v [1];
}

#define MID_DIST_MIN 0.25f
#define MID_DIST_MAX 2.0f

static gfloat
cell_midpoint_distance_badness (ChafaMesh *mesh, gint x, gint y)
{
    Quad corners;
    ChafaVec2f32 midpoint;
    ChafaVec2f32 initial_midpoint;
    ChafaVec2f32 ofs;
    gfloat min, max;
    gfloat badness = 0.0f;

    cell_corners (mesh, x, y, &corners);
    quad_to_grid_units (&corners, &mesh->cell_scale);
    quad_midpoint (&midpoint, &corners);
    quad_extremes (&min, &max, &midpoint, &corners);

    DEBUG (g_printerr ("before min, max = %f, %f, cell_scale = %f, %f\n",
                       min, max, mesh->cell_scale.v [0], mesh->cell_scale.v [1]));

    /* Normalize so 1.0 is starting distance to midpoint */

    min *= M_SQRT2f;
    max *= M_SQRT2f;

#if 0
    g_printerr ("min, max = %f, %f\n", min, max);
#endif

    if (min < 1.0f)
        badness += (1.0f / (MAX (min - MID_DIST_MIN, 0.000001f))) - (1.0f / (1.0f - MID_DIST_MIN));
#if 1
    if (max > 1.0f)
        badness += (1.0f / (MAX (MID_DIST_MAX - max, 0.000001f))) - (1.0f / (MID_DIST_MAX - 1.0f));
#endif
    return badness;
}

#if 0

/* Midpoint drift */

static gfloat
cell_drift_badness (ChafaMesh *mesh, gint x, gint y)
{
    Quad corners;
    ChafaVec2f32 midpoint;
    ChafaVec2f32 initial_midpoint;
    ChafaVec2f32 ofs;

    initial_midpoint.v [0] = (gfloat) x + 0.5f;
    initial_midpoint.v [1] = (gfloat) y + 0.5f;

    cell_corners (mesh, x, y, &corners);
    quad_to_grid_units (&corners, &mesh->cell_scale);
    quad_midpoint (&midpoint, &corners);

    chafa_vec2f32_sub (&ofs, &midpoint, &initial_midpoint);
    chafa_vec2f32_mul_scalar (&ofs, &ofs, 250.f);

#if 0
    g_printerr ("(%d,%d): mid=(%.1f,%.1f) ofs=(%.1f,%.1f)\n", x, y,
                midpoint.v [0], midpoint.v [1],
                ofs.v [0], ofs.v [1]);
#endif

    return chafa_vec2f32_get_squared_magnitude (&ofs);
}

#else

/* Corner drift */

static gfloat
cell_drift_badness (ChafaMesh *mesh, gint x, gint y)
{
    Quad initial_corners;
    Quad corners;
    Quad ofs;

    chafa_vec2f32_set (&initial_corners.vecs [0], x, y);
    chafa_vec2f32_set (&initial_corners.vecs [1], x + 1, y);
    chafa_vec2f32_set (&initial_corners.vecs [2], x, y + 1);
    chafa_vec2f32_set (&initial_corners.vecs [3], x + 1, y + 1);

    cell_corners (mesh, x, y, &corners);
    quad_to_grid_units (&corners, &mesh->cell_scale);

    chafa_vec2f32_sub (&ofs.vecs [0], &corners.vecs [0], &initial_corners.vecs [0]);
    chafa_vec2f32_sub (&ofs.vecs [1], &corners.vecs [1], &initial_corners.vecs [1]);
    chafa_vec2f32_sub (&ofs.vecs [2], &corners.vecs [2], &initial_corners.vecs [2]);
    chafa_vec2f32_sub (&ofs.vecs [3], &corners.vecs [3], &initial_corners.vecs [3]);

#if 0
    g_printerr ("(%d,%d): mid=(%.1f,%.1f) ofs=(%.1f,%.1f)\n", x, y,
                midpoint.v [0], midpoint.v [1],
                ofs.v [0], ofs.v [1]);
#endif

    return 400.0f * (chafa_vec2f32_get_squared_magnitude (&ofs.vecs [0])
                     + chafa_vec2f32_get_squared_magnitude (&ofs.vecs [1])
                     + chafa_vec2f32_get_squared_magnitude (&ofs.vecs [2])
                     + chafa_vec2f32_get_squared_magnitude (&ofs.vecs [3]));
}

#endif

static gfloat
cell_size_badness (ChafaMesh *mesh, gint x, gint y)
{
    Quad corners;
    ChafaVec2f32 dist [6];
    gfloat m [6];
    gfloat badness = 0.0f;
    gint i;

    cell_corners (mesh, x, y, &corners);
    quad_to_grid_units (&corners, &mesh->cell_scale);

    /* Rect */
    chafa_vec2f32_sub (&dist [0], &corners.vecs [1], &corners.vecs [0]);
    chafa_vec2f32_sub (&dist [1], &corners.vecs [2], &corners.vecs [0]);
    chafa_vec2f32_sub (&dist [2], &corners.vecs [3], &corners.vecs [1]);
    chafa_vec2f32_sub (&dist [3], &corners.vecs [3], &corners.vecs [2]);

    /* Diagonals */
    chafa_vec2f32_sub (&dist [4], &corners.vecs [3], &corners.vecs [0]);
    chafa_vec2f32_sub (&dist [5], &corners.vecs [2], &corners.vecs [1]);

    for (i = 0; i < 6; i++)
        m [i] = chafa_vec2f32_get_squared_magnitude (&dist [i]);

    m [4] /= M_SQRT2f * M_SQRT2f;
    m [4] *= m [4];
    m [5] /= M_SQRT2f * M_SQRT2f;
    m [5] *= m [5];

    /* All distances are now normalized to the range [0.0 - 1.0] */

    for (i = 0; i < 6; i++)
    {
        if (m [i] <= 1.0f)
            badness += (1.0f - m [i]) * 1000.0f;
#if 1
        /* Penalizing greater than normal distances makes it go crazy */
        else
            badness += (m [i] - 1.0f) * 1000.0f;
#endif
    }

    return badness;
}

/* How much does the middle point stick out? */
static gfloat
dtl (ChafaMesh *mesh,
     gint x0, gint y0,
     gint x1, gint y1,
     gint x2, gint y2)
{
    ChafaVec2f32 p [3];

    chafa_vec2f32_hadamard (&p [0], get_point (mesh, x0, y0), &mesh->cell_scale);
    chafa_vec2f32_hadamard (&p [1], get_point (mesh, x1, y1), &mesh->cell_scale);
    chafa_vec2f32_hadamard (&p [2], get_point (mesh, x2, y2), &mesh->cell_scale);

    return chafa_vec2f32_distance_to_line (&p [1], &p [0], &p [2]);
}

static gfloat
cell_align_badness (ChafaMesh *mesh, gint x, gint y)
{
    gfloat dist [8] = { 0 };
    gfloat r = 0.0f;
    gint i;

    if (x > 0)
    {
        dist [0] = dtl (mesh, x - 1, y, x, y, x + 1, y);
        dist [1] = dtl (mesh, x - 1, y + 1, x, y + 1, x + 1, y + 1);
    }

    if (x < mesh->grid_width - 2)
    {
        dist [2] = dtl (mesh, x, y, x + 1, y, x + 2, y);
        dist [3] = dtl (mesh, x, y + 1, x + 1, y + 1, x + 2, y + 1);
    }

    if (y > 0)
    {
        dist [4] = dtl (mesh, x, y - 1, x, y, x, y + 1);
        dist [5] = dtl (mesh, x + 1, y - 1, x + 1, y, x + 1, y + 1);
    }

    if (y < mesh->grid_height - 2)
    {
        dist [6] = dtl (mesh, x, y, x, y + 1, x, y + 2);
        dist [7] = dtl (mesh, x + 1, y, x + 1, y + 1, x + 1, y + 2);
    }

    for (i = 0; i < 8; i++)
        r += dist [i] * dist [i];

    return r;
}

gfloat
chafa_mesh_get_cell_drift (ChafaMesh *mesh, gint x, gint y)
{
    return cell_drift_badness (mesh, x, y);
}

gfloat
chafa_mesh_get_cell_deform (ChafaMesh *mesh, gint x, gint y)
{
    return cell_size_badness (mesh, x, y);
}

gfloat
chafa_mesh_get_cell_misalign (ChafaMesh *mesh, gint x, gint y)
{
    return cell_align_badness (mesh, x, y);
}

gfloat
chafa_mesh_get_relative_deformity (ChafaMesh *mesh,
                                   gint x, gint y)
{
    ChafaVec2f32 *p;
    ChafaVec2f32 v;
    gfloat value = 0.0f;
    gfloat dist;

    p = get_point (mesh, x, y);

#if 0
    if (x > 0)
        value += fabsf ((p->v [0] - get_point (mesh, x - 1, y)->v [0]) * mesh->cell_scale - 0.5f);

    if (x < mesh->grid_width - 1)
        value += fabsf ((get_point (mesh, x + 1, y)->v [0] - p->v [0]) * mesh->cell_scale - 0.5f);

    if (y > 0)
        value += fabsf ((p->v [1] - get_point (mesh, x, y - 1)->v [1]) * mesh->cell_scale - 0.5f);

    if (y < mesh->grid_height - 1)
        value += fabsf ((get_point (mesh, x, y + 1)->v [1] - p->v [1]) * mesh->cell_scale - 0.5f);

#elif 0

    value += squared_inverse_distance (mesh, p, x - 1, y - 1);
    value += squared_inverse_distance (mesh, p, x,     y - 1);
    value += squared_inverse_distance (mesh, p, x + 1, y - 1);
    value += squared_inverse_distance (mesh, p, x - 1, y);
    value += squared_inverse_distance (mesh, p, x + 1, y);
    value += squared_inverse_distance (mesh, p, x - 1, y + 1);
    value += squared_inverse_distance (mesh, p, x,     y + 1);
    value += squared_inverse_distance (mesh, p, x + 1, y + 1);

#elif 0

    value += squared_distance (mesh, p, x - 1, y - 1);
    value += squared_distance (mesh, p, x,     y - 1);
    value += squared_distance (mesh, p, x + 1, y - 1);
    value += squared_distance (mesh, p, x - 1, y);
    value += squared_distance (mesh, p, x + 1, y);
    value += squared_distance (mesh, p, x - 1, y + 1);
    value += squared_distance (mesh, p, x,     y + 1);
    value += squared_distance (mesh, p, x + 1, y + 1);

#else

    if (y > 0)
    {
        if (x > 0)
            value += cell_midpoint_distance_badness (mesh, x - 1, y - 1);
        value += cell_midpoint_distance_badness (mesh, x, y - 1);
    }
    if (x > 0)
        value += cell_midpoint_distance_badness (mesh, x - 1, y);

    value += cell_midpoint_distance_badness (mesh, x, y);

#endif

    return value;
}

#define MATRIX_ROW(u) \
    { o [u] * o [0], o [u] * o [1], o [u] * o [2], o [u] * o [3], \
      o [u] * o [4], o [u] * o [5], o [u] * o [6], o [u] * o [7], \
      o [u] * o [8] }

void
chafa_mesh_sample_cell (ChafaMesh *mesh, ChafaWorkCell *work_cell_out,
                        gint x, gint y,
                        gconstpointer pixels,
                        gint pixels_width, gint pixels_height,
                        gint pixels_rowstride)
{
    Quad corners;
    const gfloat o [9] =
    {
        0.0f / 8.0f, 1.0f / 8.0f, 2.0f / 8.0f, 3.0f / 8.0f,
        4.0f / 8.0f, 5.0f / 8.0f, 6.0f / 8.0f, 7.0f / 8.0f,
        8.0f / 8.0f
    };
    const gfloat m [9] [9] =
    {
        MATRIX_ROW (0), MATRIX_ROW (1), MATRIX_ROW (2), MATRIX_ROW (3),
        MATRIX_ROW (4), MATRIX_ROW (5), MATRIX_ROW (6), MATRIX_ROW (7),
        MATRIX_ROW (8)
    };
    const guint32 *pixels_u32 = pixels;
    gint i, j;

    cell_corners (mesh, x, y, &corners);

    for (i = 0; i < 8; i++)
    {
        for (j = 0; j < 8; j++)
        {
            ChafaVec2f32 p [4];
            ChafaVec2f32 q = CHAFA_VEC2F32_INIT_ZERO;

            chafa_vec2f32_mul_scalar (&p [0], &corners.vecs [0], m [8 - i] [8 - j]);
            chafa_vec2f32_mul_scalar (&p [1], &corners.vecs [1], m [8 - i] [j]);
            chafa_vec2f32_mul_scalar (&p [2], &corners.vecs [2], m [i] [8 - j]);
            chafa_vec2f32_mul_scalar (&p [3], &corners.vecs [3], m [i] [j]);

            chafa_vec2f32_add (&q, &q, &p [0]);
            chafa_vec2f32_add (&q, &q, &p [1]);
            chafa_vec2f32_add (&q, &q, &p [2]);
            chafa_vec2f32_add (&q, &q, &p [3]);

            q.v [0] = MIN (q.v [0], pixels_width - 1);
            q.v [1] = MIN (q.v [1], pixels_height - 1);

            chafa_unpack_color (pixels_u32 [((gint) q.v [1]) * (pixels_rowstride / 4)
                                            + ((gint ) q.v [0])],
                                &work_cell_out->pixels [i * CHAFA_SYMBOL_WIDTH_PIXELS + j].col);
        }
    }
}

static void
transform_point (ChafaMesh *mesh, ChafaVec2f32 *q, const ChafaVec2f32 *p)
{
    gint cell_x, cell_y;
    Quad corners;
    ChafaVec2f32 ofs;
    ChafaVec2f32 t [4];

    cell_x = p->v [0];
    cell_y = p->v [1];

    ofs.v [0] = p->v [0] - cell_x;
    ofs.v [1] = p->v [1] - cell_y;

    cell_corners (mesh, cell_x, cell_y, &corners);

    chafa_vec2f32_mul_scalar (&t [0], &corners.vecs [0], (1.0f - ofs.v [0]) * (1.0f - ofs.v [1]));
    chafa_vec2f32_mul_scalar (&t [1], &corners.vecs [1], ofs.v [0] * (1.0f - ofs.v [1]));
    chafa_vec2f32_mul_scalar (&t [2], &corners.vecs [2], (1.0f - ofs.v [0]) * ofs.v [1]);
    chafa_vec2f32_mul_scalar (&t [3], &corners.vecs [3], ofs.v [0] * ofs.v [1]);

    chafa_vec2f32_set_zero (q);
    chafa_vec2f32_add (q, q, &t [0]);
    chafa_vec2f32_add (q, q, &t [1]);
    chafa_vec2f32_add (q, q, &t [2]);
    chafa_vec2f32_add (q, q, &t [3]);
}

void
chafa_mesh_oversample_cell (ChafaMesh *mesh, ChafaWorkCell *work_cell_out,
                            gint x, gint y,
                            gconstpointer pixels,
                            gint pixels_width, gint pixels_height,
                            gint pixels_rowstride,
                            gfloat oversample_amt)
{
    const guint32 *pixels_u32 = pixels;
    gint i, j;

    for (i = 0; i < 8; i++)
    {
        for (j = 0; j < 8; j++)
        {
            ChafaVec2f32 p, q;

            p.v [0] = x - oversample_amt + (j + oversample_amt * 2.0f) / 7.0f;
            p.v [1] = y - oversample_amt + (i + oversample_amt * 2.0f) / 7.0f;
            p.v [0] = MAX (p.v [0], 0.0f);
            p.v [1] = MAX (p.v [1], 0.0f);

            transform_point (mesh, &q, &p);

            q.v [0] = MIN (q.v [0], pixels_width - 1);
            q.v [1] = MIN (q.v [1], pixels_height - 1);

            chafa_unpack_color (pixels_u32 [((gint) q.v [1]) * (pixels_rowstride / 4)
                                            + ((gint ) q.v [0])],
                                &work_cell_out->pixels [i * CHAFA_SYMBOL_WIDTH_PIXELS + j].col);
        }
    }
}
