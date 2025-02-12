/*
 * Copyright © 2011 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

/* Chafa note: I've changed the namespace to avoid potential future
 * conflicts. This fine piece of engineering is otherwise unmodified
 * from the GLib original. --hpj */

/* gwakeup.c is special -- GIO and some test cases include it.  As such,
 * it cannot include other glib headers without triggering the single
 * includes warnings.  We have to manually include its dependencies here
 * (and at all other use sites).
 */
#include <glib.h>
#include "chafa-wakeup.h"

/*< private >
 * SECTION:gwakeup
 * @title: ChafaWakeup
 * @short_description: portable cross-thread event signal mechanism
 *
 * #ChafaWakeup is a simple and portable way of signaling events between
 * different threads in a way that integrates nicely with g_poll().
 * GLib uses it internally for cross-thread signalling in the
 * implementation of #GMainContext and #GCancellable.
 *
 * You first create a #ChafaWakeup with chafa_wakeup_new() and initialise a
 * #GPollFD from it using chafa_wakeup_get_pollfd().  Polling on the created
 * #GPollFD will block until chafa_wakeup_signal() is called, at which point
 * it will immediately return.  Future attempts to poll will continue to
 * return until chafa_wakeup_acknowledge() is called.  chafa_wakeup_free() is
 * used to free a #ChafaWakeup.
 *
 * On sufficiently modern Linux, this is implemented using eventfd.  On
 * Windows it is implemented using an event handle.  On other systems it
 * is implemented with a pair of pipes.
 *
 * Since: 2.30
 **/
#ifdef _WIN32

#include <windows.h>

#ifdef GLIB_COMPILATION
#include "gmessages.h"
#include "giochannel.h"
#include "gwin32.h"
#endif

ChafaWakeup *
chafa_wakeup_new (void)
{
  HANDLE wakeup;

  wakeup = CreateEvent (NULL, TRUE, FALSE, NULL);

  if (wakeup == NULL)
    g_error ("Cannot create event for ChafaWakeup: %s",
             g_win32_error_message (GetLastError ()));

  return (ChafaWakeup *) wakeup;
}

void
chafa_wakeup_get_pollfd (ChafaWakeup *wakeup,
                     GPollFD *poll_fd)
{
  poll_fd->fd = (gintptr) wakeup;
  poll_fd->events = G_IO_IN;
}

void
chafa_wakeup_acknowledge (ChafaWakeup *wakeup)
{
  ResetEvent ((HANDLE) wakeup);
}

void
chafa_wakeup_signal (ChafaWakeup *wakeup)
{
  SetEvent ((HANDLE) wakeup);
}

void
chafa_wakeup_free (ChafaWakeup *wakeup)
{
  CloseHandle ((HANDLE) wakeup);
}

#else

#include "glib-unix.h"
#include <fcntl.h>

#if defined (HAVE_EVENTFD)
#include <sys/eventfd.h>
#endif

struct _ChafaWakeup
{
  gint fds[2];
};

/**
 * chafa_wakeup_new:
 *
 * Creates a new #ChafaWakeup.
 *
 * You should use chafa_wakeup_free() to free it when you are done.
 *
 * Returns: a new #ChafaWakeup
 *
 * Since: 2.30
 **/
ChafaWakeup *
chafa_wakeup_new (void)
{
  GError *error = NULL;
  ChafaWakeup *wakeup;

  wakeup = g_slice_new (ChafaWakeup);

  /* try eventfd first, if we think we can */
#if defined (HAVE_EVENTFD)
#ifndef TEST_EVENTFD_FALLBACK
  wakeup->fds[0] = eventfd (0, EFD_CLOEXEC | EFD_NONBLOCK);
#else
  wakeup->fds[0] = -1;
#endif

  if (wakeup->fds[0] != -1)
    {
      wakeup->fds[1] = -1;
      return wakeup;
    }

  /* for any failure, try a pipe instead */
#endif

  if (!g_unix_open_pipe (wakeup->fds, FD_CLOEXEC, &error))
    g_error ("Creating pipes for ChafaWakeup: %s", error->message);

  if (!g_unix_set_fd_nonblocking (wakeup->fds[0], TRUE, &error) ||
      !g_unix_set_fd_nonblocking (wakeup->fds[1], TRUE, &error))
    g_error ("Set pipes non-blocking for ChafaWakeup: %s", error->message);

  return wakeup;
}

/**
 * chafa_wakeup_get_pollfd:
 * @wakeup: a #ChafaWakeup
 * @poll_fd: a #GPollFD
 *
 * Prepares a @poll_fd such that polling on it will succeed when
 * chafa_wakeup_signal() has been called on @wakeup.
 *
 * @poll_fd is valid until @wakeup is freed.
 *
 * Since: 2.30
 **/
void
chafa_wakeup_get_pollfd (ChafaWakeup *wakeup,
                         GPollFD *poll_fd)
{
  poll_fd->fd = wakeup->fds[0];
  poll_fd->events = G_IO_IN;
}

/**
 * chafa_wakeup_acknowledge:
 * @wakeup: a #ChafaWakeup
 *
 * Acknowledges receipt of a wakeup signal on @wakeup.
 *
 * You must call this after @wakeup polls as ready.  If not, it will
 * continue to poll as ready until you do so.
 *
 * If you call this function and @wakeup is not signaled, nothing
 * happens.
 *
 * Since: 2.30
 **/
void
chafa_wakeup_acknowledge (ChafaWakeup *wakeup)
{
  char buffer[16];

  /* read until it is empty */
  while (read (wakeup->fds[0], buffer, sizeof buffer) == sizeof buffer);
}

/**
 * chafa_wakeup_signal:
 * @wakeup: a #ChafaWakeup
 *
 * Signals @wakeup.
 *
 * Any future (or present) polling on the #GPollFD returned by
 * chafa_wakeup_get_pollfd() will immediately succeed until such a time as
 * chafa_wakeup_acknowledge() is called.
 *
 * This function is safe to call from a UNIX signal handler.
 *
 * Since: 2.30
 **/
void
chafa_wakeup_signal (ChafaWakeup *wakeup)
{
  int res;

  if (wakeup->fds[1] == -1)
    {
      guint64 one = 1;

      /* eventfd() case. It requires a 64-bit counter increment value to be
       * written. */
      do
        res = write (wakeup->fds[0], &one, sizeof one);
      while (G_UNLIKELY (res == -1 && errno == EINTR));
    }
  else
    {
      guint8 one = 1;

      /* Non-eventfd() case. Only a single byte needs to be written, and it can
       * have an arbitrary value. */
      do
        res = write (wakeup->fds[1], &one, sizeof one);
      while (G_UNLIKELY (res == -1 && errno == EINTR));
    }
}

/**
 * chafa_wakeup_free:
 * @wakeup: a #ChafaWakeup
 *
 * Frees @wakeup.
 *
 * You must not currently be polling on the #GPollFD returned by
 * chafa_wakeup_get_pollfd(), or the result is undefined.
 **/
void
chafa_wakeup_free (ChafaWakeup *wakeup)
{
  close (wakeup->fds[0]);

  if (wakeup->fds[1] != -1)
    close (wakeup->fds[1]);

  g_slice_free (ChafaWakeup, wakeup);
}

#endif /* !_WIN32 */
