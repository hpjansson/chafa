/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2019-2022 Hans Petter Jansson
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
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef HAVE_MMAP
# include <sys/mman.h>
#endif

#ifdef HAVE_GETRANDOM
# include <sys/random.h>
#endif

#include <glib/gstdio.h>

#include "file-mapping.h"

#ifdef HAVE_MMAP
/* Workaround for non-Linux platforms */
# ifndef MAP_NORESERVE
#  define MAP_NORESERVE 0
# endif
#endif

/* MS Windows needs files to be explicitly opened as O_BINARY. However, this
 * flag is not always defined in our cross builds. */
#ifndef O_BINARY
# ifdef _O_BINARY
# define O_BINARY _O_BINARY
# else
# define O_BINARY 0
# endif
#endif

/* Streams bigger than this must be cached in a file */
#define FILE_MEMORY_CACHE_MAX (4 * 1024 * 1024)

/* Size of buffer used for copying stdin to file */
#define COPY_BUFFER_SIZE 8192

struct FileMapping
{
    gchar *path;
    gpointer data;
    gsize length;
    gint fd;
    guint failed : 1;
    guint is_mmapped : 1;
};

static gboolean
file_is_stdin (FileMapping *file_mapping)
{
    return file_mapping->path
        && file_mapping->path [0] == '-'
        && file_mapping->path [1] == '\0' ? TRUE : FALSE;
 }

static gsize
safe_read (gint fd, void *buf, gsize len)
{
   gsize ntotal = 0;
   guint8 *buffer = buf;

   while (len > 0)
   {
       unsigned int nread;
       int iread;

       /* Passing nread > INT_MAX to read is implementation defined in POSIX
        * 1003.1, therefore despite the unsigned argument portable code must
        * limit the value to INT_MAX!
        */
       if (len > INT_MAX)
           nread = INT_MAX;
       else
           nread = (unsigned int)/*SAFE*/len;

       iread = read (fd, buffer, nread);

       if (iread == -1)
       {
           /* This is the devil in the details, a read can terminate early with 0
            * bytes read because of EINTR, yet it still returns -1 otherwise end
            * of file cannot be distinguished.
            */
           if (errno != EINTR)
           {
               /* I.e. a permanent failure */
               return 0;
           }
       }
       else if (iread < 0)
       {
           /* Not a valid 'read' result: */
           return 0;
       }
       else if (iread > 0)
       {
           /* Continue reading until a permanent failure, or EOF */
           buffer += iread;
           len -= (unsigned int)/*SAFE*/iread;
           ntotal += (unsigned int)/*SAFE*/iread;
       }
       else
       {
           return ntotal;
       }
   }

   return ntotal; /* len == 0 */
}

static gboolean
safe_write (gint fd, gconstpointer buf, gsize len)
{
    gsize ntotal = 0;
    const guint8 *buffer = buf;
    gboolean success = FALSE;

    while (len > 0)
    {
       guint to_write;
       gint n_written;

       if (len > INT_MAX)
           to_write = INT_MAX;
       else
           to_write = (unsigned int) len;

       n_written = write (fd, buffer, to_write);

       if (n_written == -1)
       {
           if (errno != EINTR)
               goto out;
       }
       else if (n_written < 0)
       {
           /* Not a valid 'write' result */
           goto out;
       }
       else if (n_written > 0)
       {
           /* Continue writing until permanent failure or entire buffer written */
           buffer += n_written;
           len -= (unsigned int) n_written;
           ntotal += (unsigned int) n_written;
       }
    }

    success = TRUE;

out:
    return success;
}

static gboolean
safe_copy (gint src_fd, gint dest_fd)
{
    guint8 buf [COPY_BUFFER_SIZE];
    gsize n_read;

    while ((n_read = safe_read (src_fd, buf, COPY_BUFFER_SIZE)) > 0)
    {
        if (!safe_write (dest_fd, buf, n_read))
            return FALSE;
    }

    return TRUE;
}

static void
free_file_data (FileMapping *file_mapping)
{
    if (file_mapping->data)
    {
#ifdef HAVE_MMAP
        if (file_mapping->is_mmapped)
            munmap (file_mapping->data, file_mapping->length);
        else
#endif
            g_free (file_mapping->data);
    }

    if (file_mapping->fd >= 0)
        g_close (file_mapping->fd, NULL);

    file_mapping->fd = -1;
    file_mapping->data = NULL;
    file_mapping->is_mmapped = FALSE;
}

static guint64
get_random_u64 (void)
{
    guint64 u64 = 0;
    guint32 *u32p = (guint32 *) &u64;
    gint len = 0;
    GTimeVal tv;

#ifdef HAVE_GETRANDOM
    len = getrandom ((void *) &u64, sizeof (guint64), GRND_NONBLOCK);
#endif

    if (!u64 || len < (gint) sizeof (guint64))
    {
        gpointer p;

        /* FIXME: Not paranoid enough */

        /* FIXME: g_get_current_time() was deprecated in GLib 2.62. Switch to
         * g_get_real_time () sometime, and require GLib >= 2.28.  */

        g_get_current_time (&tv);
        g_random_set_seed (tv.tv_sec ^ tv.tv_usec);
        u32p [0] ^= g_random_int ();
        g_get_current_time (&tv);
        g_random_set_seed (tv.tv_sec ^ tv.tv_usec);
        u32p [1] ^= g_random_int ();

        p = g_thread_self ();
        u64 ^= (guint64) p;
    }

    return u64;
}

static gint
open_temp_file_in_path (const gchar *base_path)
{
    gchar *cache_path;
    gint fd;

    if (!base_path)
        return -1;

    cache_path = g_strdup_printf ("%s%schafa-%016" G_GINT64_MODIFIER "x",
                                  base_path,
                                  G_DIR_SEPARATOR_S,
                                  get_random_u64 ());

    /* Create the file and unlink it, so it goes away when we exit */
    fd = g_open (cache_path, O_CREAT | O_RDWR | O_BINARY, S_IRUSR | S_IWUSR);
    if (fd >= 0)
    {
        /* You can't unlink an open file on MS Windows, so this will fail there */
        g_unlink (cache_path);
    }

    g_free (cache_path);

    return fd;
}

static gint
open_temp_file (void)
{
    gint fd;

    fd = open_temp_file_in_path (g_get_user_cache_dir ());
    if (fd < 0)
        fd = open_temp_file_in_path (g_get_tmp_dir ());

    return fd;
}

static gint
cache_stdin (FileMapping *file_mapping, GError **error)
{
    gpointer buf = NULL;
    gint stdin_fd = fileno (stdin);  /* Can't use STDIN_FILENO on Windows */
    size_t n_read;
    gint cache_fd = -1;
    gint success = FALSE;

    if (stdin_fd < 0)
        goto out;

    /* Read from stdin */

    buf = g_malloc (FILE_MEMORY_CACHE_MAX);
    n_read = safe_read (stdin_fd, buf, FILE_MEMORY_CACHE_MAX);
    if (n_read < 1)
        goto out;

    /* If the stream didn't fit in memory, save it all to a file instead.
     * We can mmap it later. Or read it back in, ha ha. */

    if (n_read == FILE_MEMORY_CACHE_MAX)
    {
        cache_fd = open_temp_file ();
        if (cache_fd >= 0)
        {
            if (!safe_write (cache_fd, buf, n_read))
                goto out;
            g_free (buf);
            buf = NULL;

            if (!safe_copy (stdin_fd, cache_fd))
                goto out;
        }
    }
    else
    {
        file_mapping->data = g_realloc (buf, n_read);
        file_mapping->length = n_read;
        buf = NULL;
    }

    success = TRUE;

out:
    if (!success)
    {
        if (cache_fd >= 0)
            g_close (cache_fd, NULL);
        cache_fd = -1;
        g_free (buf);

        if (error && !*error)
        {
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                         "Could not cache input stream");
        }
    }

    return cache_fd;
}

static gint
open_file (FileMapping *file_mapping, GError **error)
{
    gint fd;

    if (file_is_stdin (file_mapping))
    {
        fd = cache_stdin (file_mapping, error);
    }
    else
    {
        fd = g_open (file_mapping->path, O_RDONLY | O_BINARY, S_IRUSR | S_IWUSR);
        if (fd < 0)
        {
            g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                         strerror (errno));
        }
    }

    return fd;
}

static gboolean
ensure_open_file (FileMapping *file_mapping)
{
    if (file_mapping->data || file_mapping->fd >= 0)
        return TRUE;

    file_mapping->fd = open_file (file_mapping, NULL);
    if (file_mapping->data || file_mapping->fd >= 0)
        return TRUE;

    return FALSE;
}

static guint8 *
read_file (gint fd, size_t *data_size)
{
    struct stat sb;
    guint8 *buffer;
    size_t size;
    size_t n;

    if (fstat (fd, &sb)) {
        return NULL;
    }

    size = sb.st_size;

    buffer = g_try_malloc (size);
    if (!buffer) {
        g_printerr ("Unable to allocate %lld bytes\n",
                    (long long) size);
        return NULL;
    }

    if (lseek (fd, 0, SEEK_SET) != 0)
    {
        g_free (buffer);
        return NULL;
    }

    n = safe_read (fd, buffer, size);
    if (n != size)
    {
        g_free (buffer);
        return NULL;
    }

    *data_size = size;
    return buffer;
}

static gboolean
map_or_read_file (FileMapping *file_mapping)
{
    gboolean result = FALSE;
    struct stat sbuf;
    gint t;

    if (file_mapping->failed)
        return FALSE;
    if (file_mapping->data)
        return TRUE;

    if (file_mapping->fd < 0)
        file_mapping->fd = open_file (file_mapping, NULL);

    /* If the data arrived over a pipe and was reasonably small, open_file () will
     * populate the data fields instead of the fd. */
    if (!file_mapping->data)
    {
        if (file_mapping->fd < 0)
            goto out;

        t = fstat (file_mapping->fd, &sbuf);

        if (!t)
        {
            file_mapping->length = sbuf.st_size;
#ifdef HAVE_MMAP
            /* Try memory mapping first */
            file_mapping->data = mmap (NULL,
                                       file_mapping->length,
                                       PROT_READ,
                                       MAP_SHARED | MAP_NORESERVE,
                                       file_mapping->fd,
                                       0);
#endif
        }

        if (file_mapping->data)
        {
            file_mapping->is_mmapped = TRUE;
        }
        else
        {
            file_mapping->data = read_file (file_mapping->fd, &file_mapping->length);
            file_mapping->is_mmapped = FALSE;
        }
    }

    if (!file_mapping->data)
        goto out;

    result = TRUE;

out:

    if (!result)
    {
        file_mapping->failed = TRUE;
    }

    return result;
}

FileMapping *
file_mapping_new (const gchar *path)
{
    FileMapping *file_mapping;

    file_mapping = g_new0 (FileMapping, 1);
    file_mapping->path = g_strdup (path);
    file_mapping->fd = -1;

    return file_mapping;
}

void
file_mapping_destroy (FileMapping *file_mapping)
{
    free_file_data (file_mapping);
    g_free (file_mapping->path);
    g_free (file_mapping);
}

gboolean
file_mapping_open_now (FileMapping *file_mapping, GError **error)
{
    if (file_mapping->data || file_mapping->fd >= 0)
        return TRUE;

    file_mapping->fd = open_file (file_mapping, error);
    if (file_mapping->data || file_mapping->fd >= 0)
        return TRUE;

    if (error && !*error)
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                     "Open/map failed");
    }

    return FALSE;
}

const gchar *
file_mapping_get_path (FileMapping *file_mapping)
{
    return file_mapping->path;
}

gconstpointer
file_mapping_get_data (FileMapping *file_mapping, gsize *length_out)
{
    if (!file_mapping->data)
        map_or_read_file (file_mapping);

    if (length_out)
        *length_out = file_mapping->length;

    return file_mapping->data;
}

gboolean
file_mapping_taste (FileMapping *file_mapping, gpointer out, goffset ofs, gsize length)
{
    if (!ensure_open_file (file_mapping))
        return FALSE;

    if (file_mapping->data)
    {
        if (ofs + length <= file_mapping->length)
        {
            memcpy (out, ((const gchar *) file_mapping->data) + ofs, length);
            return TRUE;
        }

        return FALSE;
    }

    if (file_mapping->fd < 0)
        return FALSE;

    if (lseek (file_mapping->fd, ofs, SEEK_SET) != ofs)
        return FALSE;

    if (safe_read (file_mapping->fd, out, length) != length)
        return FALSE;

    return TRUE;
}

gssize
file_mapping_read (FileMapping *file_mapping, gpointer out, goffset ofs, gsize length)
{
    if (!ensure_open_file (file_mapping))
        return FALSE;

    if (file_mapping->data)
    {
        if (ofs <= (gssize) file_mapping->length)
        {
            gssize seg_len = MIN (length, file_mapping->length - ofs);
            memcpy (out, ((const gchar *) file_mapping->data) + ofs, seg_len);
            return seg_len;
        }

        return -1;
    }

    /* FIXME: Shouldn't happen */
    if (file_mapping->fd < 0)
        return -1;

    if (lseek (file_mapping->fd, ofs, SEEK_SET) != ofs)
        return -1;

    return safe_read (file_mapping->fd, out, length);
}

gboolean
file_mapping_has_magic (FileMapping *file_mapping, goffset ofs, gconstpointer data, gsize length)
{
    gchar *buf;

    if (!ensure_open_file (file_mapping))
        return FALSE;

    if (file_mapping->data)
    {
        if (ofs + length <= file_mapping->length
            && !memcmp ((const guint8 *) file_mapping->data + ofs, data, length))
            return TRUE;

        return FALSE;
    }

    /* FIXME: Shouldn't happen */
    if (file_mapping->fd < 0)
        return FALSE;

    if (lseek (file_mapping->fd, ofs, SEEK_SET) != ofs)
        return FALSE;

    buf = alloca (length);

    if (safe_read (file_mapping->fd, buf, length) != length)
        return FALSE;

    if (memcmp (buf, data, length))
        return FALSE;

    return TRUE;
}
