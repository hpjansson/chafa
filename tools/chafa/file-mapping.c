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

#include "config.h"
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_MMAP
# include <sys/mman.h>
#endif

#include "file-mapping.h"

#ifdef HAVE_MMAP
/* Workaround for non-Linux platforms */
# ifndef MAP_NORESERVE
#  define MAP_NORESERVE 0
# endif
#endif

struct FileMapping
{
    gchar *path;
    gpointer data;
    gsize length;
    gint fd;
    guint is_mmapped : 1;
};

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
           return ntotal;
   }

   return ntotal; /* len == 0 */
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
        close (file_mapping->fd);

    file_mapping->fd = -1;
    file_mapping->data = NULL;
    file_mapping->is_mmapped = FALSE;
}

static gint
open_file (FileMapping *file_mapping)
{
    return open (file_mapping->path, O_RDONLY);
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

    if (file_mapping->data)
        return TRUE;

    if (file_mapping->fd < 0)
        file_mapping->fd = open_file (file_mapping);

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

    if (!file_mapping->data)
        goto out;

    result = TRUE;

out:
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
    if (file_mapping->fd < 0)
        file_mapping->fd = open_file (file_mapping);

    if (file_mapping->fd < 0)
        return FALSE;

    if (lseek (file_mapping->fd, ofs, SEEK_SET) != ofs)
        return FALSE;

    if (safe_read (file_mapping->fd, out, length) != length)
        return FALSE;

    return TRUE;
}

gboolean
file_mapping_has_magic (FileMapping *file_mapping, goffset ofs, gconstpointer data, gsize length)
{
    gchar *buf;

    if (file_mapping->data)
    {
        if (ofs + length <= file_mapping->length
            && !memcmp ((const guint8 *) data + ofs, data, length))
            return TRUE;

        return FALSE;
    }

    if (file_mapping->fd < 0)
        file_mapping->fd = open_file (file_mapping);

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
