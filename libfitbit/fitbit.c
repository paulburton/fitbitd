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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ant.h>
#include "fitbit.h"
#include "util.h"

#define LOG_TAG "fitbit"
#include "log.h"

struct fitbit_s {
    ant_t *ant;
    uint8_t chan;
    uint8_t packet_id, packet_id_counter;
    uint8_t bank_id;

    uint8_t curr_dev_num[2];
    uint8_t skipped_setups, max_skipped_setups;
};

typedef struct {
    fitbit_cb_foundbase *found_base;
    void *user;
    int found;
} fitbit_ant_state_t;

static int fitbit_init_ant_channel(fitbit_t *fb, uint8_t dev_num[2])
{
    uint8_t net_key[8] = { 0 };
    uint8_t period[2] = { 0x00, 0x10 };
    uint8_t msg_id;
    struct timespec ts;
    int attempts;

    if (!memcmp(dev_num, fb->curr_dev_num, sizeof(fb->curr_dev_num))) {
        if (fb->skipped_setups++ < fb->max_skipped_setups) {
            DBG("ANT channel already setup\n");
            return 0;
        }
    }
    DBG("init ANT channel %d dev_num 0x%02x 0x%02x\n", fb->chan,
        dev_num[0], dev_num[1]);

    /* ensure failure will cause a retry */
    memset(fb->curr_dev_num, 0, sizeof(fb->curr_dev_num));

    /* reset the base */
    CHAINERR_LTZ(ant_reset(fb->ant), err);

    /* reset takes 500ms */
    memset(&ts, 0, sizeof(ts));
    ts.tv_nsec = 500 * 1000000; /* 500ms */
    nanosleep(&ts, NULL);

    /* startup message */
    attempts = 10;
    ts.tv_nsec = 100 * 1000000; /* 100ms */
    while (attempts--) {
        if (!ant_receive(fb->ant, &msg_id, NULL, NULL, 0)) {
            if (msg_id == 0x6f) {
                /* got startup message */
                break;
            }
        }
        nanosleep(&ts, NULL);
    }

    /* channel init */
    CHAINERR_LTZ(ant_set_network_key(fb->ant, fb->chan, net_key), err);
    CHAINERR_LTZ(ant_assign_channel(fb->ant, fb->chan, 0, 0), err);
    CHAINERR_LTZ(ant_set_channel_period(fb->ant, fb->chan, period), err);
    CHAINERR_LTZ(ant_set_channel_freq(fb->ant, fb->chan, 2), err);
    CHAINERR_LTZ(ant_set_tx_power(fb->ant, 3), err);
    CHAINERR_LTZ(ant_set_channel_search_timeout(fb->ant, fb->chan, 0xff), err);
    CHAINERR_LTZ(ant_set_channel_id(fb->ant, fb->chan, dev_num, 1, 1), err);
    CHAINERR_LTZ(ant_open_channel(fb->ant, fb->chan), err);

    /* all done, record dev num */
    memcpy(fb->curr_dev_num, dev_num, sizeof(fb->curr_dev_num));
    fb->skipped_setups = 0;

    return 0;
err:
    return -1;
}

static int fitbit_find_tracker_beacon(fitbit_t *fb)
{
    struct timespec ts;
    int attempts;
    uint8_t msg_id = 0;

    /* look for tracker beacon */
    attempts = 50;
    memset(&ts, 0, sizeof(ts));
    ts.tv_nsec = 100 * 1000000; /* 100ms */
    while (attempts--) {
        if (!ant_receive(fb->ant, &msg_id, NULL, NULL, 0)) {
            if (msg_id == 0x4e) {
                /* broadcast from tracker */
                return 0;
            }
        }
        nanosleep(&ts, NULL);
    }

    /* no tracker beacon */
    return -1;
}

static uint8_t fitbit_packet_id(fitbit_t *fb)
{
    uint8_t curr;
    curr = fb->packet_id_counter++;
    fb->packet_id_counter %= 8;
    fb->packet_id = 0x38 + curr;
    return fb->packet_id;
}

static int fitbit_tracker_receive_burst(fitbit_t *fb, uint8_t *buf, size_t sz, size_t *len)
{
    uint8_t burstbuf[0xffff + 8];
    size_t burstlen, datalen;

    CHAINERR_LTZ(ant_receive_burst(fb->ant, fb->chan, burstbuf, sizeof(burstbuf), &burstlen), err);

    if (burstlen < 1 || burstbuf[1] != 0x81) {
        ERR("not a tracker burst\n");
        goto err;
    }

    datalen = (burstbuf[3] << 8) | burstbuf[2];
    DBG("tracker burst %d bytes\n", (int)datalen);

    memcpy(buf, &burstbuf[8], MIN(datalen, sz));
    if (len)
        *len = MIN(datalen, sz);

    return 0;
err:
    return -1;
}

static int fitbit_tracker_send_burst(fitbit_t *fb, uint8_t *buf, size_t sz)
{
    uint8_t cksum, *burstbuf;
    int i;

    burstbuf = malloc(8 + sz);
    if (!burstbuf)
        goto oom;

    /* calculate checksum */
    cksum = 0;
    for (i = 0; i < sz; i++)
        cksum ^= buf[i];

    /* fill out initial packet */
    memset(burstbuf, 0, 8);
    burstbuf[0] = fitbit_packet_id(fb);
    burstbuf[1] = 0x80;
    burstbuf[2] = sz & 0xff;
    burstbuf[3] = (sz >> 8) & 0xff;
    burstbuf[7] = cksum;

    /* and the rest are data */
    memcpy(&burstbuf[8], buf, sz);

    CHAINERR_LTZ(ant_send_burst(fb->ant, fb->chan, burstbuf, 8 + sz), err_send);

    free(burstbuf);
    return 0;

err_send:
    free(burstbuf);
oom:
    return -1;
}

static int fitbit_get_data_bank(fitbit_t *fb, uint8_t *buf, size_t sz, size_t *bank_len)
{
    uint8_t data[8];

    DBG("reading data bank\n");

    if (bank_len)
        *bank_len = 0;

    /* request bank */
    memset(&data, 0, sizeof(data));
    data[0] = fitbit_packet_id(fb);
    data[1] = 0x70;
    data[3] = 0x02;
    data[4] = fb->bank_id++;
    CHAINERR_LTZ(ant_send_acked_data(fb->ant, fb->chan, data), err);

    /* read data */
    CHAINERR_LTZ(fitbit_tracker_receive_burst(fb, buf, sz, bank_len), err);
    DBG("got whole data bank\n");
    return 0;

err:
    return -1;
}

int fitbit_run_op(fitbit_t *fb, uint8_t op[7], uint8_t *payload, size_t payload_sz, uint8_t *response, size_t response_sz, size_t *response_len)
{
    uint8_t data[8];
    int attempts = 10;
    size_t len;

    if (response_len)
        *response_len = 0;

    while (attempts--) {
        data[0] = fitbit_packet_id(fb);
        memcpy(&data[1], op, 7);
        CHAINERR_LTZ(ant_send_acked_data(fb->ant, fb->chan, data), err_attempt);

        CHAINERR_LTZ(ant_receive_acked_response(fb->ant, fb->chan, data, sizeof(data)), err_attempt);
        if (data[0] != fb->packet_id) {
            ERR("invalid packet ID 0x%02x\n", data[0]);
            goto err_attempt;
        }

        DBG("got acked response for ID 0x%02x\n", data[0]);

        if (data[1] == 0x41) {
            /* use response data */
            if (response) {
                len = MIN(response_sz, 6);
                memcpy(response, &data[2], len);
                if (response_len)
                    *response_len = len;
            }
            return 0;
        }

        if (data[1] == 0x42) {
            /* use banked data */
            CHAINERR_LTZ(fitbit_get_data_bank(fb, response, response_sz, response_len), err_attempt);
            return 0;
        }

        if (data[1] == 0x61) {
            /* request payload */
            if (!payload || !payload_sz) {
                ERR("op requires payload\n");
                return -1;
            }
            /* send payload */
            CHAINERR_LTZ(fitbit_tracker_send_burst(fb, payload, payload_sz), err_attempt);
            /* get response */
            CHAINERR_LTZ(ant_receive_acked_response(fb->ant, fb->chan, data, sizeof(data)), err_attempt);
            /* use response data */
            if (response) {
                len = MIN(response_sz, 6);
                memcpy(response, &data[2], len);
                if (response_len)
                    *response_len = len;
            }
            return 0;
        }

        /* unknown */

err_attempt:
        continue;
    }

    return -1;
}

static int fitbit_sync_single_tracker(fitbit_t *fb, fitbit_cb_sync *do_sync, void *user)
{
    uint8_t data[8];
    uint8_t dev_num[2];
    uint8_t op[7];
    uint8_t info[12];
    fitbit_tracker_info_t tracker;

    /* begin at packet ID 0x39 */
    fb->packet_id_counter = 1;

    /* reset tracker */
    memset(data, 0, sizeof(data));
    data[0] = 0x78;
    data[1] = 0x01;
    CHAINERR_LTZ(ant_send_acked_data(fb->ant, fb->chan, data), err);

    /* generate a device number to use for sync */
    dev_num[0] = rand() % 0xff;
    dev_num[1] = rand() % 0xff;

    DBG("sync tracker using device number 0x%02x 0x%02x\n", dev_num[0], dev_num[1]);

    /* inform tracker of new device number */
    memset(data, 0, sizeof(data));
    data[0] = 0x78;
    data[1] = 0x02;
    data[2] = dev_num[0];
    data[3] = dev_num[1];
    CHAINERR_LTZ(ant_send_acked_data(fb->ant, fb->chan, data), err);

    /* close channel used to find tracker */
    CHAINERR_LTZ(ant_close_channel(fb->ant, fb->chan), err);

    /* reinitialise channel with new device number */
    CHAINERR_LTZ(fitbit_init_ant_channel(fb, dev_num), err);

    /* wait for the tracker beacon */
    CHAINERR_LTZ(fitbit_find_tracker_beacon(fb), err);

    /* ping tracker */
    memset(data, 0, sizeof(data));
    data[0] = 0x78;
    data[1] = 0x00;
    CHAINERR_LTZ(ant_send_acked_data(fb->ant, fb->chan, data), err);

    /* get tracker info */
    memset(op, 0, sizeof(op));
    op[0] = 0x24;
    CHAINERR_LTZ(fitbit_run_op(fb, op, NULL, 0, info, sizeof(info), NULL), err);

    memset(&tracker, 0, sizeof(tracker));
    memcpy(&tracker.serial, &info[0], 5);
    tracker.firmware = info[5];
    memcpy(&tracker.ver_bsl, &info[6], 2);
    memcpy(&tracker.ver_app, &info[8], 2);
    tracker.on_charger = !!info[11];
    snprintf(tracker.serial_str, sizeof(tracker.serial_str), "%02x%02x%02x%02x%02x",
             info[0], info[1], info[2], info[3], info[4]);

    INFO("Tracker:\n");
    INFO("    Serial: %02x%02x%02x%02x%02x\n", info[0], info[1], info[2], info[3], info[4]);
    INFO("  Firmware: %d\n", info[5]);
    INFO("       BSL: %d.%d\n", info[6], info[7]);
    INFO("       App: %d.%d\n", info[8], info[9]);
    INFO("  Charging: %s\n", info[11] ? "yes" : "no");

    if (do_sync) {
        DBG("invoking user sync\n");
        do_sync(fb, &tracker, user);
    }

    return 0;
err:
    return -1;
}

static void fitbit_found_ant_node(ant_t *ant, void *user)
{
    fitbit_ant_state_t *state = user;
    fitbit_t *fb;

    fb = calloc(1, sizeof(*fb));
    if (!fb) {
        ERR("failed to malloc fitbit\n");
        return;
    }

    fb->ant = ant;
    fb->packet_id_counter = 1;
    fb->max_skipped_setups = 10;

    state->found++;
    state->found_base(fb, state->user);
}

int fitbit_find_bases(fitbit_cb_foundbase *found_base, void *user)
{
    fitbit_ant_state_t state;

    state.found_base = found_base;
    state.user = user;
    state.found = 0;

    ant_find_nodes(fitbit_found_ant_node, &state);

    return state.found;
}

void fitbit_destroy(fitbit_t *fb)
{
    ant_destroy(fb->ant);
    free(fb);
}

void fitbit_set_max_setup_skip(fitbit_t *fb, uint8_t max_skip)
{
    fb->max_skipped_setups = max_skip;
}

int fitbit_sync_trackers(fitbit_t *fb, fitbit_cb_sync *do_sync, void *user)
{
    uint8_t dev_num[2];
    int ret, count = 0;

    while (true) {
        /* start on dev_num 0xffff to find trackers */
        dev_num[0] = dev_num[1] = 0xff;
        CHAINERR_LTZ(fitbit_init_ant_channel(fb, dev_num), err);

        /* look for tracker beacon */
        ret = fitbit_find_tracker_beacon(fb);
        if (ret < 0) {
            /* no beacon found */
            break;
        }

        if (fitbit_sync_single_tracker(fb, do_sync, user)) {
            /* sync failed */
            break;
        }

        /* synced a tracker on this iteration */
        count++;
    }

    /* it's possible we failed because the base disconnected/errored */
    if (ant_is_dead(fb->ant))
        goto err;

    return count;
err:
    return -1;
}

int fitbit_tracker_sleep(fitbit_t *fb, uint32_t duration)
{
    uint8_t data[8];

    /* reset tracker */
    memset(data, 0, sizeof(data));
    data[0] = 0x7f;
    data[1] = 0x03;
    data[7] = duration / 15; /* multiples of 15s */
    CHAINERR_LTZ(ant_send_acked_data(fb->ant, fb->chan, data), err);

    return 0;
err:
    return -1;
}

int fitbit_tracker_set_chatter(fitbit_t *fb, char *greeting, char *msg[3])
{
    uint8_t op[7];
    uint8_t payload[0x40];
    int i;

    /* setup op */
    memset(op, 0, sizeof(op));
    op[0] = 0x23;
    op[2] = sizeof(payload);

    /* setup payload */
    memset(payload, 0, sizeof(payload));
    payload[4] = 0xe2;
    payload[5] = 0x02;
    payload[6] = 0x9d;
    payload[7] = 0x03;
    payload[8] = 0x48;
    payload[9] = 0x2f;
    payload[10] = 0x52;
    payload[11] = 0x09;
    payload[12] = 0x5b;
    payload[13] = 0x3e;
    payload[21] = 0xff;

    /* greeting message */
    if (strlen(greeting) > 8)
        return -1;
    strncpy((char*)&payload[24], greeting, 10);

    /* chatter messages */
    for (i = 0; i < 3; i++) {
        if (strlen(msg[i]) > 8)
            return -1;
        strncpy((char*)&payload[34 + (i * 10)], msg[i], 10);
    }

    return fitbit_run_op(fb, op, payload, sizeof(payload), NULL, 0, NULL);
}
