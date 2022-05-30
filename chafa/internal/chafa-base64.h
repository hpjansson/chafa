/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2021-2022 Hans Petter Jansson
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

#ifndef __CHAFA_BASE64_H__
#define __CHAFA_BASE64_H__

#include <glib.h>
#include "chafa.h"

G_BEGIN_DECLS

typedef struct
{
    /* We turn 3-byte groups into 4-character base64 groups, so we
     * may need to buffer up to 2 bytes between batches */
    guint8 buf [2];
    gint buf_len;
}
ChafaBase64;

void chafa_base64_init (ChafaBase64 *base64);
void chafa_base64_deinit (ChafaBase64 *base64);

void chafa_base64_encode (ChafaBase64 *base64, GString *gs_out, gconstpointer in, gint in_len);
void chafa_base64_encode_end (ChafaBase64 *base64, GString *gs_out);

G_END_DECLS

#endif /* __CHAFA_BASE64_H__ */
