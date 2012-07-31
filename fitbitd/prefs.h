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

#ifndef __prefs_h__
#define __prefs_h__

#include <stdint.h>

typedef struct {
    uint32_t scan_delay;
    uint32_t sync_delay;
    char *upload_url;
    char *client_id;
    char *client_version;
    char *os_name;
    char *lock_filename;
    char *dump_directory;
    char *log_filename;
} fitbitd_prefs_t;

fitbitd_prefs_t *prefs_create(void);
void prefs_destroy(fitbitd_prefs_t *prefs);

#endif /* __prefs_h__ */
