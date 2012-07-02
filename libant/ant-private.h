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

#ifndef __ant_private_h__
#define __ant_private_h__

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include "ant.h"

struct ant_s {
    char name[10];

    uint8_t recvbuf[512];
    size_t recvbuf_sz;

    /* true if an unrecoverable error has occurred */
    bool dead;

    /* transport functions */
    void (*destroy)(ant_t *ant);
    ssize_t (*read)(ant_t *ant, uint8_t *buf, size_t sz);
    ssize_t (*write)(ant_t *ant, uint8_t *buf, size_t sz);
};

#endif /* __ant_private_h__ */
