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

#ifndef __ant_message_h__
#define __ant_message_h__

typedef struct {
    uint8_t id;
	uint8_t len;
	uint8_t *data;
} ant_message_t;

ant_message_t *ant_message_create(uint8_t id, uint8_t len);
ant_message_t *ant_message_vcreate(uint8_t id, ...);
void ant_message_destroy(ant_message_t *msg);

int ant_message_encode(ant_message_t *msg, uint8_t *buf, size_t sz, size_t *len);
ant_message_t *ant_message_decode(uint8_t *buf, size_t sz, size_t *len);

#endif /* __ant_message_h__ */
