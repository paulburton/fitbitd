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

#ifndef __ant_usb_h__
#define __ant_usb_h__

#include <libusb.h>
#include "ant.h"
#include "ant-private.h"

typedef struct {
    ant_t ant;
    libusb_device_handle *dev;
    int ep;
} antusb_t;

int ant_usb_find_nodes(ant_cb_foundnode *found_node, void *user);

#endif /* __ant_usb_h__ */
