/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2019-2024 Hans Petter Jansson
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

#ifndef __CHAFA_VECTOR_H__
#define __CHAFA_VECTOR_H__

#include <math.h>
#include <glib.h>

G_BEGIN_DECLS

/* --- 2D vectors --- */

#define CHAFA_VEC2F32_INIT(x, y) { { (x), (y) } }
#define CHAFA_VEC2F32_INIT_ZERO CHAFA_VEC2F32_INIT (0.0f, 0.0f)

typedef struct
{
    gfloat v [2];
}
ChafaVec2f32;

static inline void
chafa_vec2f32_set (ChafaVec2f32 *v, gfloat x, gfloat y)
{
    v->v [0] = x;
    v->v [1] = y;
}

static inline void
chafa_vec2f32_set_zero (ChafaVec2f32 *v)
{
    chafa_vec2f32_set (v, 0.0f, 0.0f);
}

static inline void
chafa_vec2f32_copy (ChafaVec2f32 *dest, const ChafaVec2f32 *src)
{
    *dest = *src;
}

static inline void
chafa_vec2f32_add (ChafaVec2f32 *out, const ChafaVec2f32 *a, const ChafaVec2f32 *b)
{
    out->v [0] = a->v [0] + b->v [0];
    out->v [1] = a->v [1] + b->v [1];
}

static inline void
chafa_vec2f32_sub (ChafaVec2f32 *out, const ChafaVec2f32 *a, const ChafaVec2f32 *b)
{
    out->v [0] = a->v [0] - b->v [0];
    out->v [1] = a->v [1] - b->v [1];
}

static inline void
chafa_vec2f32_mul_scalar (ChafaVec2f32 *out, const ChafaVec2f32 *in, gfloat s)
{
    out->v [0] = in->v [0] * s;
    out->v [1] = in->v [1] * s;
}

static inline gfloat
chafa_vec2f32_dot (const ChafaVec2f32 *v, const ChafaVec2f32 *u)
{
    return v->v [0] * u->v [0] + v->v [1] * u->v [1];
}

static inline gfloat
chafa_vec2f32_cross (const ChafaVec2f32 *a, const ChafaVec2f32 *b)
{
    return a->v [0] * b->v [1] - a->v [1] * b->v [0];
}

static inline void
chafa_vec2f32_hadamard (ChafaVec2f32 *out, const ChafaVec2f32 *a, const ChafaVec2f32 *b)
{
    out->v [0] = a->v [0] * b->v [0];
    out->v [1] = a->v [1] * b->v [1];
}

static inline gfloat
chafa_vec2f32_get_magnitude (const ChafaVec2f32 *v)
{
    return sqrtf (v->v [0] * v->v [0] + v->v [1] * v->v [1]);
}

static inline gfloat
chafa_vec2f32_get_squared_magnitude (const ChafaVec2f32 *v)
{
    return v->v [0] * v->v [0] + v->v [1] * v->v [1];
}

static inline void
chafa_vec2f32_add_from_array (ChafaVec2f32 *accum, const ChafaVec2f32 *v, gint n)
{
    gint i;

    for (i = 0; i < n; i++)
    {
        chafa_vec2f32_add (accum, accum, &v [i]);
    }
}

static inline void
chafa_vec2f32_add_to_array (ChafaVec2f32 *v, const ChafaVec2f32 *in, gint n)
{
    gint i;

    for (i = 0; i < n; i++)
    {
        chafa_vec2f32_add (&v [i], &v [i], in);
    }
}

static inline void
chafa_vec2f32_average_array (ChafaVec2f32 *out, const ChafaVec2f32 *v, gint n)
{
    chafa_vec2f32_set_zero (out);
    chafa_vec2f32_add_from_array (out, v, n);
    chafa_vec2f32_mul_scalar (out, out, 1.0f / (gfloat) n);
}

/* a0 = Start point a, a1 = vector relative to a0.
   b0 = Start point b, b1 = vector relative to b0. */
static inline gint
chafa_vec2f32_intersect_segments (ChafaVec2f32 *out,
                                  const ChafaVec2f32 *a0, const ChafaVec2f32 *a1,
                                  const ChafaVec2f32 *b0, const ChafaVec2f32 *b1)
{
    ChafaVec2f32 c;
    gfloat u [2];

    chafa_vec2f32_sub (&c, b0, a0);
    u [0] = chafa_vec2f32_cross (&c, a1);
    u [1] = chafa_vec2f32_cross (a1, b1);

    if (u [1] == .0f)
    {
        /* Colinear (possibly overlapping) or parallel and non-intersecting*/
        return -1;
    }

    u [0] /= u [1];

    if (u [0] < .0f || u [0] > 1.f)
    {
        /* Not parallel and non-intersecting */
        return 1;
    }

    chafa_vec2f32_mul_scalar (out, b1, u [0]);
    chafa_vec2f32_add (out, out, b0);
    return 0;
}

static inline gfloat
chafa_vec2f32_distance_to_line (const ChafaVec2f32 *p,
                                const ChafaVec2f32 *a0, const ChafaVec2f32 *a1)
{
    ChafaVec2f32 b;
    ChafaVec2f32 c;
    gfloat n, d;

    chafa_vec2f32_sub (&b, a1, a0);
    chafa_vec2f32_sub (&c, a0, p);
    n = chafa_vec2f32_cross (&b, &c);
    d = sqrtf (b.v [0] * b.v [0] + b.v [1] * b.v [1]);

    return n / d;
}

/* --- 3D vectors --- */

#define CHAFA_VEC3F32_INIT(x, y, z) { { (x), (y), (z) } }
#define CHAFA_VEC3F32_INIT_ZERO CHAFA_VEC3F32_INIT (0.0f, 0.0f, 0.0f)

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
chafa_vec3f32_sub (ChafaVec3f32 *out, const ChafaVec3f32 *a, const ChafaVec3f32 *b)
{
    out->v [0] = a->v [0] - b->v [0];
    out->v [1] = a->v [1] - b->v [1];
    out->v [2] = a->v [2] - b->v [2];
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

static inline gfloat
chafa_vec3f32_sum_to_scalar (const ChafaVec3f32 *v)
{
    return v->v [0] + v->v [1] + v->v [2];
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
chafa_vec3f32_hadamard (ChafaVec3f32 *out, const ChafaVec3f32 *v, const ChafaVec3f32 *u)
{
    out->v [0] = v->v [0] * u->v [0];
    out->v [1] = v->v [1] * u->v [1];
    out->v [2] = v->v [2] * u->v [2];
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
chafa_vec3f32_round (ChafaVec3f32 *out, const ChafaVec3f32 *in)
{
    out->v [0] = rintf (in->v [0]);
    out->v [1] = rintf (in->v [1]);
    out->v [2] = rintf (in->v [2]);
}

static inline void
chafa_vec3f32_average_array (ChafaVec3f32 *out, const ChafaVec3f32 *v, gint n)
{
    chafa_vec3f32_set_zero (out);
    chafa_vec3f32_add_from_array (out, v, n);
    chafa_vec3f32_mul_scalar (out, out, 1.0f / (gfloat) n);
}

G_END_DECLS

#endif /* __CHAFA_VECTOR_H__ */
