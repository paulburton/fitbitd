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

#ifndef __fitbit_h__
#define __fitbit_h__

#include <stdint.h>

typedef struct fitbit_s fitbit_t;

typedef struct {
    uint8_t serial[5];
    uint8_t firmware;
    uint8_t ver_app[2];
    uint8_t ver_bsl[2];
    bool on_charger;
    char serial_str[11];
} fitbit_tracker_info_t;

typedef void (fitbit_cb_foundbase)(fitbit_t *fb, void *user);
typedef void (fitbit_cb_sync)(fitbit_t *fb, fitbit_tracker_info_t *tracker, void *user);

int fitbit_find_bases(fitbit_cb_foundbase *found_base, void *user);
void fitbit_destroy(fitbit_t *fb);
void fitbit_set_max_setup_skip(fitbit_t *fb, uint8_t max_skip);
int fitbit_sync_trackers(fitbit_t *fb, fitbit_cb_sync *do_sync, void *user);
int fitbit_run_op(fitbit_t *fb, uint8_t op[7], uint8_t *payload, size_t payload_sz, uint8_t *response, size_t response_sz, size_t *response_len);
int fitbit_tracker_sleep(fitbit_t *fb, uint32_t duration);
int fitbit_tracker_set_chatter(fitbit_t *fb, char *greeting, char *msg[3]);

#endif /* __fitbit_h__ */
