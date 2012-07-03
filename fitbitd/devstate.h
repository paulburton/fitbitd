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

#ifndef __devstate_h__
#define __devstate_h__

#include <stdint.h>

typedef struct devstate_priv_s devstate_priv_t;

typedef struct devstate_s {
    /* tracker info */
    uint8_t serial[5];
    long last_sync_time;
    uint32_t state;

    /* filled in from fitbit server */
    char tracker_id[20];
    char user_id[20];

    /* private, devstate.c only */
    devstate_priv_t *priv;
} devstate_t;

enum {
    DEV_STATE_SYNCING = 1 << 0,
};

void devstate_enum_devices(void (*callback)(devstate_t *dev, void *user), void *user);
void devstate_record(uint8_t serial[5], void (*callback)(devstate_t *dev, void *user), void *user);
void devstate_clean(long discard_prior_to);

#endif /* __devstate_h__ */
