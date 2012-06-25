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

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ant-message.h"
#include "ant-private.h"
#include "util.h"

#define LOG_TAG "ant-message"
#include "log.h"

ant_message_t *ant_message_create(uint8_t id, uint8_t len)
{
    ant_message_t *msg;

    msg = calloc(1, sizeof(*msg));
    if (!msg)
        goto oom_msg;

    msg->id = id;
    msg->len = len;

    if (len) {
        msg->data = malloc(len);
        if (!msg->data)
            goto oom_data;
    }

    return msg;
oom_data:
    free(msg);
oom_msg:
    return NULL;
}

ant_message_t *ant_message_vcreate(uint8_t id, ...)
{
    uint8_t buf[128];
    va_list ap;
    int i, idx = 0;
    ant_message_t *msg;

    va_start(ap, id);

    while (true) {
        i = va_arg(ap, int);
        ASSERT((unsigned)i <= 0xff || i == -1);

        if (i == -1)
            break;

        buf[idx++] = (uint8_t)i;
        ASSERT(idx < sizeof(buf));
    }

    va_end(ap);

    msg = ant_message_create(id, idx);

    if (msg)
        memcpy(msg->data, buf, idx);

    return msg;
}

void ant_message_destroy(ant_message_t *msg)
{
    free(msg->data);
    free(msg);
}

int ant_message_encode(ant_message_t *msg, uint8_t *buf, size_t sz, size_t *len)
{
    uint8_t cksum = 0;
    int i;

    if (sz < (msg->len + 4)) {
        DBG("buffer too small\n");
        return -1;
    }

    buf[0] = 0xa4;
    buf[1] = msg->len;
    buf[2] = msg->id;
    memcpy(&buf[3], msg->data, msg->len);

    for (i = 0; i < (3 + msg->len); i++)
        cksum ^= buf[i];
    buf[3+msg->len] = cksum;

    if (len)
        *len = 4 + msg->len;

    return 0;
}

ant_message_t *ant_message_decode(uint8_t *buf, size_t sz, size_t *len)
{
    uint8_t *dat = buf, msgid, *msgdata, cksum;
    size_t rem = sz, datalen, skipped = 0;
    ant_message_t *msg;
    int i;

    if (len)
        *len = 0;

    /* find SYNC */
    if (rem < 1) {
        DBG("missing SYNC\n");
        return NULL;
    }
    while (rem >= 1 && *dat != 0xa4) {
        dat++;
        rem--;
        skipped++;
    }
    if (len)
        *len = skipped;
    if (skipped) {
        DBG("skipped %d bytes\n", (int)skipped);
    }

    if (rem < 1 || *dat != 0xa4) {
        DBG("missing or invalid SYNC\n");
        return NULL;
    }
    dat++; rem--;

    /* get message length */
    if (rem < 1) {
        DBG("missing length\n");
        return NULL;
    }
    datalen = *dat;
    dat++; rem--;

    /* get message ID */
    if (rem < 1) {
        DBG("missing ID\n");
        return NULL;
    }
    msgid = *dat;
    dat++; rem--;

    /* check message length */
    if (rem < datalen) {
        DBG("missing data\n");
        return NULL;
    }
    msgdata = dat;
    dat += datalen; rem -= datalen;

    /* checksum */
    if (rem < 1) {
        DBG("missing checksum\n");
        return NULL;
    }

    if (len)
        *len = skipped + 4 + datalen;

    /* check checksum */
    cksum = 0;
    for (i = 0; i < (sz - rem); i++)
        cksum ^= buf[i];
    if (cksum != *dat) {
        DBG("invalid checksum %02x vs %02x\n", *dat, cksum);
        return NULL;
    }

    msg = ant_message_create(msgid, datalen);
    if (!msg)
        return NULL;

    memcpy(msg->data, msgdata, datalen);
    return msg;
}
