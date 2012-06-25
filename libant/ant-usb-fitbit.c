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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ant-private.h"
#include "ant-usb.h"
#include "ant-usb-fitbit.h"
#include "util.h"

#define LOG_TAG "ant-usb-fitbit"
#include "log.h"

int ant_usb_fitbit_init(antusb_t *usbant)
{
    uint8_t buf[4096];

    usbant->ep = 1;

    DBG("init %s\n", usbant->ant.name);

    CHAINERR_LTZ(libusb_reset_device(usbant->dev), err);

    CHAINERR_LTZ(libusb_control_transfer(usbant->dev, 0x40, 0, 0xffff, 0, NULL, 0, 0), err);
    CHAINERR_LTZ(libusb_control_transfer(usbant->dev, 0x40, 1, 0x2000, 0, NULL, 0, 0), err);

    CHAINERR_LTZ(libusb_control_transfer(usbant->dev, 0xc0, 255, 0x370b, 0, buf, 1, 0), err);
    if (buf[0] != 0x02) {
        ERR("Received incorrect value\n");
        return -1;
    }

    CHAINERR_LTZ(libusb_control_transfer(usbant->dev, 0x40, 0, 0x0000, 0, NULL, 0, 0), err);
    CHAINERR_LTZ(libusb_control_transfer(usbant->dev, 0x40, 0, 0xffff, 0, NULL, 0, 0), err);
    CHAINERR_LTZ(libusb_control_transfer(usbant->dev, 0x40, 1, 0x2000, 0, NULL, 0, 0), err);

    CHAINERR_LTZ(libusb_control_transfer(usbant->dev, 0xc0, 255, 0x370b, 0, buf, 1, 0), err);
    if (buf[0] != 0x02) {
        ERR("Received incorrect value\n");
        return -1;
    }

    CHAINERR_LTZ(libusb_control_transfer(usbant->dev, 0x40, 1, 0x004a, 0, NULL, 0, 0), err);

    CHAINERR_LTZ(libusb_control_transfer(usbant->dev, 0xc0, 255, 0x370b, 0, buf, 1, 0), err);
    if (buf[0] != 0x02) {
        ERR("Received incorrect value\n");
        return -1;
    }

    CHAINERR_LTZ(libusb_control_transfer(usbant->dev, 0x40, 3, 0x0800, 0, NULL, 0, 0), err);

    memset(buf, 0, sizeof(buf));
    buf[0] = 0x08;
    buf[4] = 0x40;
    CHAINERR_LTZ(libusb_control_transfer(usbant->dev, 0x41, 19, 0x0000, 0, buf, 16, 0), err);

    CHAINERR_LTZ(libusb_control_transfer(usbant->dev, 0x40, 18, 0x000c, 0, NULL, 0, 0), err);

    usbant->ant.read(&usbant->ant, buf, 4096);

    return 0;

err:
    return -1;
}
