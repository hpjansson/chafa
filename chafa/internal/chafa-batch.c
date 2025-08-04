/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2019-2025 Hans Petter Jansson
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

#include <string.h>
#include <glib.h>

#include "chafa.h"
#include "internal/chafa-batch.h"

#define DEBUG(x)

#define N_THREADS_OVERSHOOT 2

static GMutex chafa_batch_mutex;
static GCond chafa_batch_cond;
static gint chafa_batch_n_threads_waiting;
static gint chafa_batch_n_threads_active;

static gint
allocate_threads (gint max_threads, gint n_batches)
{
    gint n_threads_inactive;
    gint n_threads;

    /* GThreadPool supports sharing threads between pools, but there is no
     * mechanism to manage the global thread count. If the batch API is being
     * called from multiple threads, we risk creating N * M workers, which
     * can result in hundreds of threads.
     *
     * Therefore, we use a mutex to gatekeep the global thread count. We
     * allocate threads greedily to maximize intra-batch parallelism (good for
     * animations and few/intermittent stills), with a small overshoot factor
     * to permute the allocation over time if there are lots of incoming
     * batches. In the latter case, most batches will eventually be assigned a
     * single thread each, which is ideal for inter-batch parallelism. */

    g_atomic_int_inc (&chafa_batch_n_threads_waiting);
    g_mutex_lock (&chafa_batch_mutex);

    while (chafa_batch_n_threads_active >= max_threads + N_THREADS_OVERSHOOT)
        g_cond_wait (&chafa_batch_cond, &chafa_batch_mutex);

    n_threads_inactive = max_threads - chafa_batch_n_threads_active
        - chafa_batch_n_threads_waiting + 1;
    n_threads_inactive = MAX (n_threads_inactive, 1);
    n_threads = MIN (n_threads_inactive, n_batches);

    chafa_batch_n_threads_active += n_threads;
    DEBUG (g_printerr ("ChafaBatch active threads: %d (+%d)\n", chafa_batch_n_threads_active, n_threads));

    g_atomic_int_dec_and_test (&chafa_batch_n_threads_waiting);
    g_mutex_unlock (&chafa_batch_mutex);

    return n_threads;
}

static void
deallocate_threads (gint max_threads, gint n_threads)
{
    g_mutex_lock (&chafa_batch_mutex);
    chafa_batch_n_threads_active -= n_threads;

    if (chafa_batch_n_threads_active + n_threads >= max_threads
        && chafa_batch_n_threads_active < max_threads)
    {
        g_cond_broadcast (&chafa_batch_cond);
    }

    g_mutex_unlock (&chafa_batch_mutex);
}

void
chafa_process_batches (gpointer ctx, GFunc batch_func, GFunc post_func, gint n_rows, gint n_batches, gint batch_unit)
{
    GThreadPool *thread_pool = NULL;
    ChafaBatchInfo *batches;
    gint max_threads;
    gint n_threads;
    gint n_units;
    gfloat units_per_batch;
    gfloat ofs [2] = { .0f, .0f };
    gint i;

    g_assert (n_batches >= 1);
    g_assert (batch_unit >= 1);

    if (n_rows < 1)
        return;

    max_threads = chafa_get_n_actual_threads ();
    n_threads = allocate_threads (max_threads, n_batches);

    n_units = (n_rows + batch_unit - 1) / batch_unit;
    units_per_batch = (gfloat) n_units / (gfloat) n_batches;

    batches = g_new0 (ChafaBatchInfo, n_batches);

    if (n_threads >= 2)
    {
        thread_pool = g_thread_pool_new (batch_func,
                                         (gpointer) ctx,
                                         n_threads,
                                         FALSE,
                                         NULL);
    }

    /* Divide work up into batches that are multiples of batch_unit, except
     * for the last one (if n_rows is not itself a multiple) */

    for (i = 0; i < n_batches; )
    {
        ChafaBatchInfo *batch;
        gint row_ofs [2];

        row_ofs [0] = ofs [0];

        do
        {
            ofs [1] += units_per_batch;
            row_ofs [1] = ofs [1];
        }
        while (row_ofs [0] == row_ofs [1]);

        row_ofs [0] *= batch_unit;
        row_ofs [1] *= batch_unit;

        if (row_ofs [1] > n_rows || i == n_batches - 1)
        {
            ofs [1] = n_rows + 0.5;
            row_ofs [1] = n_rows;
        }

        if (row_ofs [0] >= row_ofs [1])
        {
            /* Save the number of batches actually produced to use in
             * post_func loop later. */
            n_batches = i;
            break;
        }

        batch = &batches [i++];
        batch->first_row = row_ofs [0];
        batch->n_rows = row_ofs [1] - row_ofs [0];

#if 0
        g_printerr ("Batch %d: %04d rows\n", i, batch->n_rows);
#endif

        if (n_threads >= 2)
        {
            g_thread_pool_push (thread_pool, batch, NULL);
        }
        else
        {
            batch_func (batch, ctx);
        }

        ofs [0] = ofs [1];
    }

    if (n_threads >= 2)
    {
        /* Wait for threads to finish */
        g_thread_pool_free (thread_pool, FALSE, TRUE);
    }

    if (post_func)
    {
        for (i = 0; i < n_batches; i++)
        {
            ((void (*)(ChafaBatchInfo *, gpointer)) post_func) (&batches [i], ctx);
        }
    }

    g_free (batches);
    deallocate_threads (max_threads, n_threads);
}
