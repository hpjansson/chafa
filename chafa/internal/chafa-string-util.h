/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2020-2021 Hans Petter Jansson
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

#ifndef __CHAFA_STRING_UTIL_H__
#define __CHAFA_STRING_UTIL_H__

#include <string.h>
#include <glib.h>

G_BEGIN_DECLS

extern const char chafa_ascii_dec_u8 [256] [4];

/* Will overwrite 4 bytes starting at dest. Returns a pointer to the first
 * byte after the formatted ASCII decimal number (dest + 1..3). */
static inline gchar *
chafa_format_dec_u8 (gchar *dest, guint8 n)
{
    memcpy (dest, &chafa_ascii_dec_u8 [n] [0], 4);
    return dest + chafa_ascii_dec_u8 [n] [3];
}

/* Will overwrite 4 bytes starting at dest. Returns a pointer to the first
 * byte after the formatted ASCII decimal number (dest + 1..4). */
gchar *chafa_format_dec_uint_0_to_9999 (char *dest, guint arg);

G_END_DECLS

#endif /* __CHAFA_STRING_UTIL_H__ */
