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

#ifndef __CHAFA_BITFIELD_H__
#define __CHAFA_BITFIELD_H__

G_BEGIN_DECLS

typedef struct
{
    guint32 *bits;
    guint n_bits;
}
ChafaBitfield;

static inline void
chafa_bitfield_init (ChafaBitfield *bitfield, guint n_bits)
{
    bitfield->n_bits = n_bits;
    bitfield->bits = g_malloc0 ((n_bits + 31) / 8);
}

static inline void
chafa_bitfield_deinit (ChafaBitfield *bitfield)
{
    g_free (bitfield->bits);
    bitfield->n_bits = 0;
    bitfield->bits = NULL;
}

static inline void
chafa_bitfield_clear (ChafaBitfield *bitfield)
{
    memset (bitfield->bits, 0, (bitfield->n_bits + 31) / 8);
}

static inline gboolean
chafa_bitfield_get_bit (const ChafaBitfield *bitfield, guint nth)
{
    gint index;
    gint shift;

    g_return_val_if_fail (nth < bitfield->n_bits, FALSE);

    index = (nth / 32);
    shift = (nth % 32);

    return (bitfield->bits [index] >> shift) & 1U;
}

static inline void
chafa_bitfield_set_bit (ChafaBitfield *bitfield, guint nth, gboolean value)
{
    gint index;
    gint shift;
    guint32 v32;

    g_return_if_fail (nth < bitfield->n_bits);

    index = (nth / 32);
    shift = (nth % 32);
    v32 = (guint32) !!value;

    bitfield->bits [index] = (bitfield->bits [index] & ~(1UL << shift)) | (v32 << shift);
}

G_END_DECLS

#endif /* __CHAFA_BITFIELD_H__ */
