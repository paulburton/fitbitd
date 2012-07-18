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
#include "ant-private.h"
#include "ant-usb.h"
#include "ant-usb-fitbit.h"
#include "util.h"

#define LOG_TAG "ant-usb"
#include "log.h"

static struct {
    uint16_t vid;
    uint16_t pid;
    int (*init)(antusb_t *usbant);
} usb_devices[] = {
    { 0x10c4, 0x84c4, ant_usb_fitbit_init },
};

typedef struct devlist_s {
    libusb_device *dev;
    antusb_t *usbant;
    struct devlist_s *prev, *next;
} devlist_t;

static int _id = 0;
static devlist_t *opendevices;
static libusb_context *_usb;

static ssize_t ant_usb_read(ant_t *ant, uint8_t *buf, size_t sz)
{
    antusb_t *usbant = (antusb_t*)ant;
    int ret, trans;

    ret = libusb_bulk_transfer(usbant->dev, usbant->ep | LIBUSB_ENDPOINT_IN, buf, sz, &trans, 100);
    if (ret) {
        if (ret != LIBUSB_ERROR_TIMEOUT) {
            DBG("bulk read failure %d\n", ret);
            ant->dead = true;
        }
        return -1;
    }

    return trans;
}

static ssize_t ant_usb_write(ant_t *ant, uint8_t *buf, size_t sz)
{
    antusb_t *usbant = (antusb_t*)ant;
    int ret, trans;
    size_t rem = sz;

    while (rem) {
        ret = libusb_bulk_transfer(usbant->dev, usbant->ep | LIBUSB_ENDPOINT_OUT, buf, sz, &trans, 100);
        if (ret) {
            DBG("bulk write failure %d\n", ret);
            return -1;
        }

        rem -= trans;
    }

    return sz;
}

static void ant_usb_destroy(ant_t *ant)
{
    antusb_t *usbant = (antusb_t*)ant;
    devlist_t *devlist;
    bool removed = false;

    DBG("destroy %s\n", ant->name);

    if (usbant->dev)
        libusb_close(usbant->dev);

    for (devlist = opendevices; devlist; devlist = devlist->next) {
        if (devlist->usbant != usbant)
            continue;

        removed = true;

        /* remove from the list of open devices */
        if (devlist == opendevices) {
            opendevices = devlist->next;
            if (opendevices)
                opendevices->prev = NULL;
        } else {
            devlist->prev->next = devlist->next;
            if (devlist->next)
                devlist->next->prev = devlist->prev;
        }

        /* cleanup */
        free(devlist);

        break;
    }

    free(usbant);

    if (removed && !opendevices) {
        DBG("no open devices, cleaning up libusb\n");
        libusb_exit(_usb);
    }
}

int ant_usb_find_nodes(ant_cb_foundnode *found_node, void *user)
{
    libusb_device **list;
    struct libusb_device_descriptor desc;
    antusb_t *usbant;
    ssize_t count, i;
    int ret, usbidx, found = 0;
    int (*fn_init)(antusb_t *usbant);
    devlist_t *devlist;

    if (!opendevices) {
        DBG("initialising libusb\n");
        ret = libusb_init(&_usb);
        if (ret) {
            ERR("failed to init libusb\n");
            _usb = NULL;
            goto out;
        }
    }

    count = libusb_get_device_list(_usb, &list);
    if (count < 0)
        goto out;

    for (i = 0; i < count; i++) {
        for (devlist = opendevices; devlist; devlist = devlist->next) {
            if (devlist->dev == list[i])
                break;
        }
        if (devlist) {
            DBG("skipping open device\n");
            continue;
        }

        ret = libusb_get_device_descriptor(list[i], &desc);
        if (ret) {
            ERR("failed to get device descriptor\n");
            continue;
        }

        fn_init = NULL;
        for (usbidx = 0; usbidx < ARRAY_LENGTH(usb_devices); usbidx++) {
            if (desc.idVendor != usb_devices[usbidx].vid)
                continue;
            if (desc.idProduct != usb_devices[usbidx].pid)
                continue;
            fn_init = usb_devices[usbidx].init;
            break;
        }

        if (!fn_init)
            continue;

        usbant = calloc(1, sizeof(*usbant));
        if (!usbant)
            goto dev_err;

        snprintf(usbant->ant.name, sizeof(usbant->ant.name), "antusb%d", _id++);

        usbant->ant.destroy = ant_usb_destroy;
        usbant->ant.read = ant_usb_read;
        usbant->ant.write = ant_usb_write;

        ret = libusb_open(list[i], &usbant->dev);
        if (ret)
            goto dev_err;

        ret = fn_init(usbant);
        if (ret)
            goto dev_err;

        /* add to open device list */
        devlist = malloc(sizeof(*devlist));
        devlist->dev = list[i];
        devlist->usbant = usbant;
        devlist->prev = NULL;
        devlist->next = opendevices;
        if (devlist->next)
            devlist->next->prev = devlist;
        opendevices = devlist;

        found++;
        found_node(&usbant->ant, user);
        continue;
dev_err:
        ERR("failed to init device\n");
        if (usbant)
            usbant->ant.destroy(&usbant->ant);
    }

out:
    libusb_free_device_list(list, 1);

    if (!opendevices && _usb) {
        DBG("opened no devices, cleaning up libusb\n");
        libusb_exit(_usb);
    }

    return found;
}
