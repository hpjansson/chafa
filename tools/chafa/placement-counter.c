/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2023 Hans Petter Jansson
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
#include <stdio.h>
#include <stdlib.h>

#include <chafa.h>
#include "placement-counter.h"

#define BUF_SIZE 256

struct PlacementCounter
{
    guint id;
};

static void
save_id (PlacementCounter *counter)
{
    gchar *path = g_build_path (G_DIR_SEPARATOR_S, g_get_user_cache_dir (), "chafa", "placement-id", NULL);
    gchar buf [BUF_SIZE];

    snprintf (buf, BUF_SIZE, "%u\n", counter->id);
    g_file_set_contents (path, buf, -1, NULL);

    g_free (path);
}

static void
restore_id (PlacementCounter *counter)
{
    gchar *path = g_build_path (G_DIR_SEPARATOR_S, g_get_user_cache_dir (), "chafa", "placement-id", NULL);
    gchar *buf = NULL;
    gsize length;

    g_file_get_contents (path, &buf, &length, NULL);

    if (buf && length > 0)
    {
        counter->id = strtoul (buf, NULL, 10);
        if (counter->id < 1)
            counter->id = 1;
    }

    g_free (buf);
    g_free (path);
}

static void
ensure_id_storage (void)
{
    gchar *path = g_build_path (G_DIR_SEPARATOR_S, g_get_user_cache_dir (), "chafa", NULL);
    g_mkdir_with_parents (path, 0750);
    g_free (path);
}

PlacementCounter *
placement_counter_new (void)
{
    PlacementCounter *counter;

    counter = g_new0 (PlacementCounter, 1);
    counter->id = 0;

    ensure_id_storage ();
    restore_id (counter);

    return counter;
}

void
placement_counter_destroy (PlacementCounter *counter)
{
    save_id (counter);
    g_free (counter);
}

guint
placement_counter_get_next_id (PlacementCounter *counter)
{
    counter->id = (counter->id % 65536) + 1;
    return counter->id;
}
