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

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "ant.h"
#include "ant-message.h"
#include "ant-private.h"
#include "ant-usb.h"
#include "util.h"

#define LOG_TAG "ant"
#include "log.h"

static void dump_buffer(char *dir, uint8_t *buf, size_t sz)
{
#if DEBUG == 1
    int i;

    fprintf(stderr, "%s", dir);

    for (i = 0; i < sz; i++) {
        fprintf(stderr, " 0x%02x", buf[i]);
        if (i%16 == 15)
            fprintf(stderr, "\n");
    }
    if (i%16)
        fprintf(stderr, "\n");
#endif
}

static int ant_send_message(ant_t *ant, ant_message_t *msg)
{
    uint8_t buf[256+4];
    ssize_t written;
    size_t len;
    int ret;

    ret = ant_message_encode(msg, buf, sizeof(buf), &len);
    if (ret)
        return ret;

    dump_buffer(">>", buf, len);

    written = ant->write(ant, buf, len);
    if (written != len) {
        ERR("write failed\n");
        return -1;
    }

    return 0;
}

static ant_message_t *ant_read_message(ant_t *ant)
{
    ssize_t bytes;
    size_t len;
    ant_message_t *msg;

    /* TODO: cyclic buffer? */

    do {
        msg = NULL;
        if (ant->recvbuf_sz) {
            msg = ant_message_decode(ant->recvbuf, ant->recvbuf_sz, &len);
            if (len) {
                memmove(ant->recvbuf, &ant->recvbuf[len], ant->recvbuf_sz - len);
                ant->recvbuf_sz -= len;
            }
        }
        if (msg)
            return msg;

        bytes = ant->read(ant, &ant->recvbuf[ant->recvbuf_sz], sizeof(ant->recvbuf) - ant->recvbuf_sz);
        if (bytes > 0) {
            dump_buffer("<<", &ant->recvbuf[ant->recvbuf_sz], bytes);
            ant->recvbuf_sz += bytes;
        }
    } while (bytes > 0);

    return NULL;
}

static int ant_read_response(ant_t *ant, uint8_t msg_id, uint8_t *code)
{
    int attempts = 20;
    struct timespec ts;
    ant_message_t *msg = NULL;

    memset(&ts, 0, sizeof(ts));
    ts.tv_nsec = 100 * 1000000; /* 100ms */

    while (attempts--) {
        msg = ant_read_message(ant);
        if (!msg) {
            nanosleep(&ts, NULL);
            continue;
        }

        if (msg->id != 0x40) {
            ant_message_destroy(msg);
            msg = NULL;
            nanosleep(&ts, NULL);
            continue;
        }

        if (msg->len < 3) {
            ERR("response too short\n");
            goto err;
        }

        if (msg->data[1] != msg_id) {
            if (msg->data[1] != 1) {
                /* not an RF event */
                ERR("response for wrong id=0x%02x\n", msg->data[1]);
            }
            goto err;
        }

        /* valid response */

        if (code)
            *code = msg->data[2];

        ant_message_destroy(msg);
        msg = NULL;
        return 0;
    }

err:
    if (msg != NULL)
        ant_message_destroy(msg);
    return -1;
}

static int ant_check_ok(ant_t *ant, uint8_t msg_id)
{
    int ret, attempts = 20;
    struct timespec ts;
    uint8_t code;

    memset(&ts, 0, sizeof(ts));
    ts.tv_nsec = 100 * 1000000; /* 100ms */

    while (attempts--) {
        ret = ant_read_response(ant, msg_id, &code);
        if (ret) {
            nanosleep(&ts, NULL);
            continue;
        }

        if (code) {
            ERR("response code %d\n", code);
            goto err;
        }

        /* all good */
        break;
    }

    return 0;
err:
    return -1;
}

int ant_find_nodes(ant_cb_foundnode *found_node, void *user)
{
    int count = 0;

    count += ant_usb_find_nodes(found_node, user);

    return count;
}

void ant_destroy(ant_t *ant)
{
    ant->destroy(ant);
}

bool ant_is_dead(ant_t *ant)
{
    return ant->dead;
}

int ant_receive(ant_t *ant, uint8_t *msg_id, uint8_t *len, uint8_t *buf, size_t sz)
{
    ant_message_t *msg;

    msg = ant_read_message(ant);
    if (!msg)
        return -1;

    DBG("received message 0x%02x\n", msg->id);

    if (msg_id)
        *msg_id = msg->id;
    if (len)
        *len = msg->len;
    if (buf)
        memcpy(buf, msg->data, sz < msg->len ? sz : msg->len);

    ant_message_destroy(msg);
    return 0;
}

int ant_poll(ant_t *ant)
{
    int count = 0;

    while (!ant_receive(ant, NULL, NULL, NULL, 0))
        count++;

    return count;
}

int ant_unassign_channel(ant_t *ant, uint8_t chan)
{
    ant_message_t *msg;

    CHAINERR_NULL(msg, ant_message_create(0x41, 1), err);
    msg->data[0] = chan;

    CHAINERR_LTZ(ant_send_message(ant, msg), err);
    ant_message_destroy(msg);
    msg = NULL;
    CHAINERR_LTZ(ant_check_ok(ant, 0x41), err);

    return 0;
err:
    if (msg)
        ant_message_destroy(msg);
    return -1;
}

int ant_assign_channel(ant_t *ant, uint8_t chan, uint8_t type, uint8_t net)
{
    ant_message_t *msg = NULL;

    CHAINERR_NULL(msg, ant_message_create(0x42, 4), err);
    msg->data[0] = chan;
    msg->data[1] = type;
    msg->data[2] = net;
    msg->data[3] = 0x00; /* extended */

    CHAINERR_LTZ(ant_send_message(ant, msg), err);
    ant_message_destroy(msg);
    msg = NULL;
    CHAINERR_LTZ(ant_check_ok(ant, 0x42), err);

    return 0;
err:
    if (msg)
        ant_message_destroy(msg);
    return -1;
}

int ant_set_channel_period(ant_t *ant, uint8_t chan, uint8_t period[2])
{
    ant_message_t *msg = NULL;

    CHAINERR_NULL(msg, ant_message_create(0x43, 3), err);
    msg->data[0] = chan;
    memcpy(&msg->data[1], period, 2);

    CHAINERR_LTZ(ant_send_message(ant, msg), err);
    ant_message_destroy(msg);
    msg = NULL;
    CHAINERR_LTZ(ant_check_ok(ant, 0x43), err);

    return 0;
err:
    if (msg)
        ant_message_destroy(msg);
    return -1;
}

int ant_set_channel_search_timeout(ant_t *ant, uint8_t chan, uint8_t timeout)
{
    ant_message_t *msg = NULL;

    CHAINERR_NULL(msg, ant_message_create(0x44, 2), err);
    msg->data[0] = chan;
    msg->data[1] = timeout;

    CHAINERR_LTZ(ant_send_message(ant, msg), err);
    ant_message_destroy(msg);
    msg = NULL;
    CHAINERR_LTZ(ant_check_ok(ant, 0x44), err);

    return 0;
err:
    if (msg)
        ant_message_destroy(msg);
    return -1;
}

int ant_set_channel_freq(ant_t *ant, uint8_t chan, uint8_t freq)
{
    ant_message_t *msg = NULL;

    CHAINERR_NULL(msg, ant_message_create(0x45, 2), err);
    msg->data[0] = chan;
    msg->data[1] = freq;

    CHAINERR_LTZ(ant_send_message(ant, msg), err);
    ant_message_destroy(msg);
    msg = NULL;
    CHAINERR_LTZ(ant_check_ok(ant, 0x45), err);

    return 0;
err:
    if (msg)
        ant_message_destroy(msg);
    return -1;
}

int ant_set_network_key(ant_t *ant, uint8_t net, uint8_t key[8])
{
    ant_message_t *msg = NULL;

    CHAINERR_NULL(msg, ant_message_create(0x46, 9), err);
    msg->data[0] = net;
    memcpy(&msg->data[1], key, 8);

    CHAINERR_LTZ(ant_send_message(ant, msg), err);
    ant_message_destroy(msg);
    msg = NULL;
    CHAINERR_LTZ(ant_check_ok(ant, 0x46), err);

    return 0;
err:
    if (msg)
        ant_message_destroy(msg);
    return -1;
}

int ant_set_tx_power(ant_t *ant, uint8_t pwr)
{
    ant_message_t *msg = NULL;

    CHAINERR_NULL(msg, ant_message_create(0x47, 2), err);
    msg->data[0] = 0x00;
    msg->data[1] = pwr;

    CHAINERR_LTZ(ant_send_message(ant, msg), err);
    ant_message_destroy(msg);
    msg = NULL;
    CHAINERR_LTZ(ant_check_ok(ant, 0x47), err);

    return 0;
err:
    if (msg)
        ant_message_destroy(msg);
    return -1;
}

int ant_reset(ant_t *ant)
{
    ant_message_t *msg = NULL;

    CHAINERR_NULL(msg, ant_message_create(0x4a, 1), err);
    msg->data[0] = 0x00;

    CHAINERR_LTZ(ant_send_message(ant, msg), err);
    ant_message_destroy(msg);
    msg = NULL;

    ant->recvbuf_sz = 0;

    return 0;
err:
    if (msg)
        ant_message_destroy(msg);
    return -1;
}

int ant_open_channel(ant_t *ant, uint8_t chan)
{
    ant_message_t *msg = NULL;

    CHAINERR_NULL(msg, ant_message_create(0x4b, 1), err);
    msg->data[0] = chan;

    CHAINERR_LTZ(ant_send_message(ant, msg), err);
    ant_message_destroy(msg);
    msg = NULL;
    CHAINERR_LTZ(ant_check_ok(ant, 0x4b), err);

    return 0;
err:
    if (msg)
        ant_message_destroy(msg);
    return -1;
}

int ant_close_channel(ant_t *ant, uint8_t chan)
{
    ant_message_t *msg = NULL;

    CHAINERR_NULL(msg, ant_message_create(0x4c, 1), err);
    msg->data[0] = chan;

    CHAINERR_LTZ(ant_send_message(ant, msg), err);
    ant_message_destroy(msg);
    msg = NULL;
    CHAINERR_LTZ(ant_check_ok(ant, 0x4c), err);

    return 0;
err:
    if (msg)
        ant_message_destroy(msg);
    return -1;
}

int ant_send_acked_data(ant_t *ant, uint8_t chan, uint8_t data[8])
{
    ant_message_t *msg = NULL;
    uint8_t response_code;
    int attempts;
    struct timespec ts;

    CHAINERR_NULL(msg, ant_message_create(0x4f, 9), err);
    msg->data[0] = chan;
    memcpy(&msg->data[1], data, 8);

    CHAINERR_LTZ(ant_send_message(ant, msg), err);
    ant_message_destroy(msg);
    msg = NULL;

    attempts = 20;
    memset(&ts, 0, sizeof(ts));
    ts.tv_nsec = 100 * 1000000; /* 100ms */
    while (attempts--) {
        if (ant_read_response(ant, 0x1, &response_code)) {
            nanosleep(&ts, NULL);
            continue;
        }

        if (response_code == 5) {
            /* TX complete */
            DBG("acked data TX complete\n");
            break;
        }

        if (response_code == 6) {
            /* TX failed */
            goto err;
        }
    }

    return 0;
err:
    if (msg)
        ant_message_destroy(msg);
    return -1;
}

int ant_receive_acked_response(ant_t *ant, uint8_t chan, uint8_t *data, size_t sz)
{
    ant_message_t *msg = NULL;
    int attempts = 20;
    struct timespec ts;

    memset(&ts, 0, sizeof(ts));
    ts.tv_nsec = 100 * 1000000; /* 100ms */

    while (attempts--) {
        msg = ant_read_message(ant);
        if (!msg)
            goto err_attempt_next;

        if (msg->id != 0x4f)
            goto err_attempt_destroy;

        memcpy(data, &msg->data[1], sz < (msg->len - 1) ? sz : (msg->len - 1));
        ant_message_destroy(msg);
        return 0;

err_attempt_destroy:
        ant_message_destroy(msg);
err_attempt_next:
        nanosleep(&ts, NULL);
    }

    return -1;
}

int ant_receive_burst(ant_t *ant, uint8_t chan, uint8_t *data, size_t sz, size_t *len)
{
    ant_message_t *msg = NULL;
    int attempts;
    struct timespec ts;
    size_t cpy, datarem = sz;

    memset(&ts, 0, sizeof(ts));
    ts.tv_nsec = 1 * 1000000; /* 1ms */

    while (true) {
        attempts = 20;
        while (attempts--) {
            msg = ant_read_message(ant);
            if (msg)
                break;
            nanosleep(&ts, NULL);
        }
        if (!msg)
            goto err;

        if (msg->id == 0x40) {
            /* rf event */
            if (msg->data[0] != chan) {
                DBG("message for wrong channel\n");
                goto nextmsg;
            }
            if (msg->data[2] == 6) {
                DBG("burst TX failed\n");
                goto err;
            }
        }

        if (msg->id == 0x4f) {
            /* acked data */
            cpy = MIN(datarem, msg->len - 1);
            memcpy(data + (sz - datarem), &msg->data[1], cpy);
            datarem -= cpy;
            goto burst_done;
        }

        if (msg->id == 0x50) {
            /* burst data */
            cpy = MIN(datarem, msg->len - 1);
            memcpy(data + (sz - datarem), &msg->data[1], cpy);
            datarem -= cpy;
            if (msg->data[0] & 0x80) {
                /* last packet */
                goto burst_done;
            }
        }

nextmsg:
        if (msg != NULL) {
            ant_message_destroy(msg);
            msg = NULL;
        }
        continue;

burst_done:
        DBG("burst complete\n");
        ant_message_destroy(msg);
        if (len)
            *len = sz - datarem;
        return 0;
    }

err:
    if (msg)
        ant_message_destroy(msg);
    return -1;
}

int ant_send_burst(ant_t *ant, uint8_t chan, uint8_t *data, size_t sz)
{
    ant_message_t *msg = NULL;
    uint8_t seq = 0, *dataptr = data;
    size_t currsz, rem = sz;
    struct timespec ts;

    CHAINERR_NULL(msg, ant_message_create(0x50, 9), err);

    memset(&ts, 0, sizeof(ts));
    ts.tv_nsec = 10 * 1000000; /* 10ms */

    while (rem) {
        currsz = MIN(rem, 8);

        /* channel number */
        msg->data[0] = chan;

        /* packet sequence number */
        msg->data[0] |= (seq++ << 5);
        if (seq > 3)
            seq = 1;

        /* mark last packet */
        if (rem == currsz)
            msg->data[0] |= 0x80;

        /* fill in data */
        memcpy(&msg->data[1], dataptr, currsz);
        if (currsz < 8)
            memset(&msg->data[1+currsz], 0, 8 - currsz);

        /* send packet */
        CHAINERR_LTZ(ant_send_message(ant, msg), err);

        /* pause */
        nanosleep(&ts, NULL);

        /* move along */
        dataptr += currsz;
        rem -= currsz;
    }

    ant_message_destroy(msg);
    msg = NULL;
    return 0;

err:
    if (msg)
        ant_message_destroy(msg);
    return -1;
}

int ant_set_channel_id(ant_t *ant, uint8_t chan, uint8_t dev_num[2], uint8_t dev_type, uint8_t trans_type)
{
    ant_message_t *msg = NULL;

    CHAINERR_NULL(msg, ant_message_create(0x51, 5), err);
    msg->data[0] = chan;
    memcpy(&msg->data[1], dev_num, 2);
    msg->data[3] = dev_type;
    msg->data[4] = trans_type;

    CHAINERR_LTZ(ant_send_message(ant, msg), err);
    ant_message_destroy(msg);
    msg = NULL;
    CHAINERR_LTZ(ant_check_ok(ant, 0x51), err);

    return 0;
err:
    if (msg)
        ant_message_destroy(msg);
    return -1;
}
