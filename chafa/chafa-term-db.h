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

#ifndef __CHAFA_TERM_DB_H__
#define __CHAFA_TERM_DB_H__

#if !defined (__CHAFA_H_INSIDE__) && !defined (CHAFA_COMPILATION)
# error "Only <chafa.h> can be included directly."
#endif

#include <chafa-term-info.h>

G_BEGIN_DECLS

typedef struct ChafaTermDb ChafaTermDb;

CHAFA_AVAILABLE_IN_1_6
ChafaTermDb *chafa_term_db_new (void);
CHAFA_AVAILABLE_IN_1_6
ChafaTermDb *chafa_term_db_copy (const ChafaTermDb *term_db);
CHAFA_AVAILABLE_IN_1_6
void chafa_term_db_ref (ChafaTermDb *term_db);
CHAFA_AVAILABLE_IN_1_6
void chafa_term_db_unref (ChafaTermDb *term_db);

CHAFA_AVAILABLE_IN_1_6
ChafaTermDb *chafa_term_db_get_default (void);

CHAFA_AVAILABLE_IN_1_6
ChafaTermInfo *chafa_term_db_detect (ChafaTermDb *term_db, gchar **envp);
CHAFA_AVAILABLE_IN_1_6
ChafaTermInfo *chafa_term_db_get_fallback_info (ChafaTermDb *term_db);

G_END_DECLS

#endif /* __CHAFA_TERM_DB_H__ */
