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

#ifndef __ant_h__
#define __ant_h__

#include <stdbool.h>
#include <stdint.h>

typedef struct ant_s ant_t;

typedef void (ant_cb_foundnode)(ant_t *ant, void *user);

int ant_find_nodes(ant_cb_foundnode *found_node, void *user);
void ant_destroy(ant_t *ant);
bool ant_is_dead(ant_t *ant);
int ant_receive(ant_t *ant, uint8_t *msg_id, uint8_t *len, uint8_t *buf, size_t sz);
int ant_poll(ant_t *ant);

int ant_unassign_channel(ant_t *ant, uint8_t chan);
int ant_assign_channel(ant_t *ant, uint8_t chan, uint8_t type, uint8_t net);
int ant_set_channel_period(ant_t *ant, uint8_t chan, uint8_t period[2]);
int ant_set_channel_search_timeout(ant_t *ant, uint8_t chan, uint8_t timeout);
int ant_set_channel_freq(ant_t *ant, uint8_t chan, uint8_t freq);
int ant_set_network_key(ant_t *ant, uint8_t net, uint8_t key[8]);
int ant_set_tx_power(ant_t *ant, uint8_t pwr);
int ant_reset(ant_t *ant);
int ant_open_channel(ant_t *ant, uint8_t chan);
int ant_close_channel(ant_t *ant, uint8_t chan);
int ant_send_acked_data(ant_t *ant, uint8_t chan, uint8_t data[8]);
int ant_receive_acked_response(ant_t *ant, uint8_t chan, uint8_t *data, size_t sz);
int ant_receive_burst(ant_t *ant, uint8_t chan, uint8_t *data, size_t sz, size_t *len);
int ant_send_burst(ant_t *ant, uint8_t chan, uint8_t *data, size_t sz);
int ant_set_channel_id(ant_t *ant, uint8_t chan, uint8_t dev_num[2], uint8_t dev_type, uint8_t trans_type);

#endif /* __ant_h__ */
