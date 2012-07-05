/*
 * This file is part of fitbitd.
 *
 * fitbitd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * fitbitd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with fitbitd.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __fitbitd_h__
#define __fitbitd_h__

#include <stdbool.h>
#include <stdint.h>

typedef struct fitbitd_tracker_priv_s fitbitd_tracker_priv_t;

typedef struct {
    char serial[11];
    char id[20];
    char user_id[20];

    bool sync_active;
    GDateTime *sync_time;

    void *user;
    fitbitd_tracker_priv_t *priv;
} fitbitd_tracker_t;

typedef void (fitbitd_tracker_callback)(fitbitd_tracker_t *tracker, void *user);
typedef void (fitbitd_change_callback)(void *user);

int fitbitd_exit(void);
int fitbitd_enum_trackers(fitbitd_tracker_callback *callback, void *user);

int fitbitd_watch_changes(fitbitd_change_callback *callback, void *user);

#endif /* __fitbitd_h__ */
