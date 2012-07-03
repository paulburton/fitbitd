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

#include <pthread.h>

#define LOG_TAG "devstate"
#include "log.h"

#include "devstate.h"

struct devstate_priv_s {
    /* doubly linked list */
    struct devstate_s *prev, *next;
};

static devstate_t *devs = NULL;
static pthread_mutex_t devs_mutex = PTHREAD_MUTEX_INITIALIZER;

void devstate_enum_devices(void (*callback)(devstate_t *dev, void *user), void *user)
{
    devstate_t *dev;

    pthread_mutex_lock(&devs_mutex);

    for (dev = devs; dev; dev = dev->priv->next)
        callback(dev, user);

    pthread_mutex_unlock(&devs_mutex);
}

void devstate_record(uint8_t serial[5], void (*callback)(devstate_t *dev, void *user), void *user)
{
    devstate_t *dev = NULL;

    pthread_mutex_lock(&devs_mutex);

    /* look for existing dev record */
    for (dev = devs; dev; dev = dev->priv->next) {
        if (!memcmp(serial, dev->serial, sizeof(dev->serial))) {
            /* found it! */
            goto out;
        }
    }

    /* alloc new devstate_t */
    dev = calloc(1, sizeof(*dev));
    if (!dev) {
        ERR("failed to alloc devstate_t\n");
        goto out;
    }

    /* alloc new devstate_priv_t */
    dev->priv = calloc(1, sizeof(devstate_priv_t));
    if (!dev->priv) {
        ERR("failed to alloc devstate_priv_t\n");
        free(dev);
        dev = NULL;
        goto out;
    }

    /* fill in serial */
    memcpy(dev->serial, serial, sizeof(dev->serial));

    /* prepend to devs list */
    dev->priv->next = devs;
    if (dev->priv->next)
        dev->priv->next->priv->prev = dev;
    devs = dev;

out:
    /* call user callback */
    if (dev && callback)
        callback(dev, user);

    pthread_mutex_unlock(&devs_mutex);
}

void devstate_clean(long discard_prior_to)
{
    devstate_t *dev;

    pthread_mutex_lock(&devs_mutex);

    for (dev = devs; dev; dev = dev->priv->next) {
        if (dev->last_sync_time >= discard_prior_to)
            continue;

        /* remove dev from devs list */
        if (dev->priv->prev)
            dev->priv->prev->priv->next = dev->priv->next;
        if (dev->priv->next)
            dev->priv->next->priv->prev = dev->priv->prev;

        /* free devstate_t */
        free(dev);
    }

    pthread_mutex_unlock(&devs_mutex);
}
