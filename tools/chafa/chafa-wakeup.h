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

#ifndef __CHAFA_WAKEUP_H__
#define __CHAFA_WAKEUP_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _ChafaWakeup ChafaWakeup;

ChafaWakeup *   chafa_wakeup_new            (void);
void            chafa_wakeup_free           (ChafaWakeup *wakeup);

void            chafa_wakeup_get_pollfd     (ChafaWakeup *wakeup,
					     GPollFD *poll_fd);
void            chafa_wakeup_signal         (ChafaWakeup *wakeup);
void            chafa_wakeup_acknowledge    (ChafaWakeup *wakeup);

G_END_DECLS

#endif
