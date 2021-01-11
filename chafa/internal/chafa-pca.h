/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2019-2021 Hans Petter Jansson
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

#ifndef __CHAFA_PCA_H__
#define __CHAFA_PCA_H__

#include <math.h>
#include <glib.h>

G_BEGIN_DECLS

#define CHAFA_VEC3F32_INIT(x, y, z) { { (x), (y), (z) } }
#define CHAFA_VEC3F32_INIT_ZERO CHAFA_VEC3F32_INIT (0.0, 0.0, 0.0)

typedef struct
{
    gfloat v [3];
}
ChafaVec3f32;

typedef struct
{
    gint32 v [3];
}
ChafaVec3i32;

static inline void
chafa_vec3i32_set (ChafaVec3i32 *out, gint32 x, gint32 y, gint32 z)
{
    out->v [0] = x;
    out->v [1] = y;
    out->v [2] = z;
}

static inline void
chafa_vec3i32_sub (ChafaVec3i32 *out, const ChafaVec3i32 *a, const ChafaVec3i32 *b)
{
    out->v [0] = a->v [0] - b->v [0];
    out->v [1] = a->v [1] - b->v [1];
    out->v [2] = a->v [2] - b->v [2];
}

static inline void
chafa_vec3i32_from_vec3f32 (ChafaVec3i32 *out, const ChafaVec3f32 *in)
{
    /* lrintf() rounding can be extremely slow, so use this function
     * sparingly. We use tricks to make GCC emit cvtss2si instructions
     * ("-fno-math-errno -fno-trapping-math", or simply "-ffast-math"),
     * but Clang apparently cannot be likewise persuaded.
     *
     * Clang _does_ like rounding with SSE 4.1, but that's not something
     * we can enable by default. */

    out->v [0] = lrintf (in->v [0]);
    out->v [1] = lrintf (in->v [1]);
    out->v [2] = lrintf (in->v [2]);
}

static inline gint32
chafa_vec3i32_dot_32 (const ChafaVec3i32 *v, const ChafaVec3i32 *u)
{
    return v->v [0] * u->v [0] + v->v [1] * u->v [1] + v->v [2] * u->v [2];
}

static inline gint64
chafa_vec3i32_dot_64 (const ChafaVec3i32 *v, const ChafaVec3i32 *u)
{
    return v->v [0] * u->v [0] + v->v [1] * u->v [1] + v->v [2] * u->v [2];
}

static inline void
chafa_vec3f32_copy (ChafaVec3f32 *dest, const ChafaVec3f32 *src)
{
    *dest = *src;
}

static inline void
chafa_vec3f32_add (ChafaVec3f32 *out, const ChafaVec3f32 *a, const ChafaVec3f32 *b)
{
    out->v [0] = a->v [0] + b->v [0];
    out->v [1] = a->v [1] + b->v [1];
    out->v [2] = a->v [2] + b->v [2];
}

static inline void
chafa_vec3f32_add_from_array (ChafaVec3f32 *accum, const ChafaVec3f32 *v, gint n)
{
    gint i;

    for (i = 0; i < n; i++)
    {
        chafa_vec3f32_add (accum, accum, &v [i]);
    }
}

static inline void
chafa_vec3f32_add_to_array (ChafaVec3f32 *v, const ChafaVec3f32 *in, gint n)
{
    gint i;

    for (i = 0; i < n; i++)
    {
        chafa_vec3f32_add (&v [i], &v [i], in);
    }
}

static inline void
chafa_vec3f32_sub (ChafaVec3f32 *out, const ChafaVec3f32 *a, const ChafaVec3f32 *b)
{
    out->v [0] = a->v [0] - b->v [0];
    out->v [1] = a->v [1] - b->v [1];
    out->v [2] = a->v [2] - b->v [2];
}

static inline void
chafa_vec3f32_mul_scalar (ChafaVec3f32 *out, const ChafaVec3f32 *in, gfloat s)
{
    out->v [0] = in->v [0] * s;
    out->v [1] = in->v [1] * s;
    out->v [2] = in->v [2] * s;
}

static inline gfloat
chafa_vec3f32_dot (const ChafaVec3f32 *v, const ChafaVec3f32 *u)
{
    return v->v [0] * u->v [0] + v->v [1] * u->v [1] + v->v [2] * u->v [2];
}

static inline void
chafa_vec3f32_set (ChafaVec3f32 *v, gfloat x, gfloat y, gfloat z)
{
    v->v [0] = x;
    v->v [1] = y;
    v->v [2] = z;
}

static inline void
chafa_vec3f32_set_zero (ChafaVec3f32 *v)
{
    chafa_vec3f32_set (v, 0.0f, 0.0f, 0.0f);
}

static inline gfloat
chafa_vec3f32_get_magnitude (const ChafaVec3f32 *v)
{
    return sqrtf (v->v [0] * v->v [0] + v->v [1] * v->v [1] + v->v [2] * v->v [2]);
}

static inline void
chafa_vec3f32_normalize (ChafaVec3f32 *out, const ChafaVec3f32 *in)
{
    gfloat m;

    m = 1.0f / chafa_vec3f32_get_magnitude (in);
    out->v [0] = in->v [0] * m;
    out->v [1] = in->v [1] * m;
    out->v [2] = in->v [2] * m;
}

static inline void
chafa_vec3f32_average_array (ChafaVec3f32 *out, const ChafaVec3f32 *v, gint n)
{
    chafa_vec3f32_set_zero (out);
    chafa_vec3f32_add_from_array (out, v, n);
    chafa_vec3f32_mul_scalar (out, out, 1.0f / (gfloat) n);
}

void chafa_vec3f32_array_compute_pca (const ChafaVec3f32 *vecs_in, gint n_vecs,
                                      gint n_components,
                                      ChafaVec3f32 *eigenvectors_out,
                                      gfloat *eigenvalues_out,
                                      ChafaVec3f32 *average_out);

G_END_DECLS

#endif /* __CHAFA_PCA_H__ */
