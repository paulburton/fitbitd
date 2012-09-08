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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <pthread.h>
#include <semaphore.h>

#include <dbus/dbus.h>

#include "control.h"
#include "devstate.h"
#include "fitbitd-utils.h"

#define LOG_TAG "control"
#include "log.h"

#define NAMESPACE "eu.paulburton.fitbitd"
#define OBJECT "/eu/paulburton/fitbitd/FitBitD"

static pthread_t control_thread;
static sem_t sem_control_init;
static volatile int control_init_ret = -1;
static volatile bool control_exit = false;
static volatile bool control_state_changed = false;

static void handle_exit(DBusMessage *msg, DBusConnection *conn)
{
    DBusMessage *reply;
    DBusMessageIter args;
    dbus_uint32_t serial, code = 0;

    reply = dbus_message_new_method_return(msg);
    if (!reply) {
        ERR("failed to create reply\n");
        goto out;
    }

    dbus_message_iter_init_append(reply, &args);
    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &code)) {
        ERR("failed to append code to reply\n");
        goto out;
    }

    if (!dbus_connection_send(conn, reply, &serial)) {
        ERR("failed to send reply\n");
        goto out;
    }

    dbus_connection_flush(conn);
    control_exit = true;

out:
    if (reply)
        dbus_message_unref(reply);
}

#define DEV_SIG "suuss"

static void callback_get_devices(devstate_t *dev, void *user)
{
    DBusMessageIter *array = user, devstruct;
    char serial_str[11];
    const char *str_ptr;
    dbus_uint32_t u32;

    /* open struct container */
    if (!dbus_message_iter_open_container(array, DBUS_TYPE_STRUCT, NULL, &devstruct)) {
        ERR("failed to open devstruct container\n");
        goto err;
    }

    /* append serial */
    snprintf(serial_str, sizeof(serial_str), "%02x%02x%02x%02x%02x",
            dev->serial[0], dev->serial[1], dev->serial[2], dev->serial[3], dev->serial[4]);
    str_ptr = serial_str;
    if (!dbus_message_iter_append_basic(&devstruct, DBUS_TYPE_STRING, &str_ptr)) {
        ERR("failed to append serial\n");
        goto err_abandon_devstruct;
    }

    /* append state */
    u32 = dev->state;
    if (!dbus_message_iter_append_basic(&devstruct, DBUS_TYPE_UINT32, &u32)) {
        ERR("failed to append state\n");
        goto err_abandon_devstruct;
    }

    /* append sync time delta */
    u32 = get_uptime() - dev->last_sync_time;
    if (!dbus_message_iter_append_basic(&devstruct, DBUS_TYPE_UINT32, &u32)) {
        ERR("failed to append sync time\n");
        goto err_abandon_devstruct;
    }

    /* append tracker ID */
    str_ptr = dev->tracker_id;
    if (!dbus_message_iter_append_basic(&devstruct, DBUS_TYPE_STRING, &str_ptr)) {
        ERR("failed to append serial\n");
        goto err_abandon_devstruct;
    }

    /* append user ID */
    str_ptr = dev->user_id;
    if (!dbus_message_iter_append_basic(&devstruct, DBUS_TYPE_STRING, &str_ptr)) {
        ERR("failed to append serial\n");
        goto err_abandon_devstruct;
    }

    if (!dbus_message_iter_close_container(array, &devstruct)) {
        ERR("failed to close devstruct container\n");
        goto err_abandon_devstruct;
    }

    return;

err_abandon_devstruct:
    dbus_message_iter_abandon_container(array, &devstruct);
err:
    ERR("failed to create devstruct for tracker\n");
}

static void handle_get_devices(DBusMessage *msg, DBusConnection *conn)
{
    DBusMessage *reply;
    DBusMessageIter args, array;
    dbus_uint32_t serial;
    bool array_open = false;

    reply = dbus_message_new_method_return(msg);
    if (!reply) {
        ERR("failed to create reply\n");
        goto out;
    }

    dbus_message_iter_init_append(reply, &args);

    /* open array of devices */
    if (!dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "(" DEV_SIG ")", &array)) {
        ERR("failed to open array container\n");
        goto out;
    }
    array_open = true;

    /* enumerate devices */
    devstate_enum_devices(callback_get_devices, &array);

    /* close array container */
    if (!dbus_message_iter_close_container(&args, &array)) {
        ERR("failed to close array container\n");
        goto out;
    }
    array_open = false;

    /* send reply message */
    if (!dbus_connection_send(conn, reply, &serial)) {
        ERR("failed to send reply\n");
        goto out;
    }

    dbus_connection_flush(conn);

out:
    if (array_open)
        dbus_message_iter_abandon_container(&args, &array);
    if (reply)
        dbus_message_unref(reply);
}

static void handle_state_changed(DBusConnection *conn)
{
    DBusMessage* msg = NULL;
    dbus_uint32_t serial;
      
    msg = dbus_message_new_signal(OBJECT, NAMESPACE ".FitBitD", "StateChanged");
    if (!msg) {
        ERR("failed to create signal message\n");
        goto out;
    }

    if (!dbus_connection_send(conn, msg, &serial)) {
        ERR("failed to signal state change\n");
        goto out;
    }

    dbus_connection_flush(conn);

out:
    if (msg)
        dbus_message_unref(msg);
}

void *control_main(void *user)
{
    DBusError err;
    DBusConnection *conn = NULL;
    DBusMessage *msg = NULL;
    int ret;

    dbus_error_init(&err);
    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) {
        ERR("failed to connect to DBUS (%s)\n", err.message);
        dbus_error_free(&err);
        sem_post(&sem_control_init);
        return NULL;
    }
    if (!conn) {
        ERR("no DBUS connection\n");
        sem_post(&sem_control_init);
        return NULL;
    }

    ret = dbus_bus_request_name(conn, NAMESPACE ".server",
            DBUS_NAME_FLAG_REPLACE_EXISTING | DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);
    if (dbus_error_is_set(&err)) {
        ERR("failed to request DBUS name (%s)\n", err.message);
        dbus_error_free(&err);
        sem_post(&sem_control_init);
        goto out;
    }
    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        ERR("not primary name owner\n");
        sem_post(&sem_control_init);
        goto out;
    }

    control_init_ret = 0;
    sem_post(&sem_control_init);

    while (!control_exit) {
        if (!dbus_connection_read_write(conn, 500)) {
            ERR("DBUS connection dead\n");
            control_exit = true;
            goto out;
        }

        msg = dbus_connection_pop_message(conn);

        if (!msg) {
            /* no message, but we might want to signal */
            goto msg_done;
        }

        if (dbus_message_is_method_call(msg, NAMESPACE ".FitBitD", "Exit"))
            handle_exit(msg, conn);
        else if (dbus_message_is_method_call(msg, NAMESPACE ".FitBitD", "GetDevices"))
            handle_get_devices(msg, conn);

        dbus_message_unref(msg);

msg_done:
        if (control_state_changed) {
            handle_state_changed(conn);
            control_state_changed = false;
        }
    }

out:
    if (conn)
        dbus_connection_unref(conn);
    return NULL;
}

int control_start(void)
{
    int ret;

    sem_init(&sem_control_init, 0, -1);

    ret = pthread_create(&control_thread, NULL, control_main, NULL);
    if (ret) {
        ERR("pthread_create failure %d\n", ret);
        return ret;
    }

    sem_wait(&sem_control_init);
    return control_init_ret;
}

void control_stop(void)
{
    if (control_init_ret) {
        /* control isn't running */
        return;
    }

    control_exit = true;
}

bool control_exited(void)
{
    if (control_init_ret) {
        /* control isn't running */
        return false;
    }

    return control_exit;
}

void control_signal_state_change(void)
{
    if (control_init_ret) {
        /* control isn't running */
        return;
    }

    control_state_changed = true;
}

int control_call_exit(void)
{
    DBusError err;
    DBusConnection *conn = NULL;
    DBusMessage *msg = NULL;
    DBusMessageIter args;
    DBusPendingCall *pending;
    int ierr, ret = -1;
    dbus_uint32_t code;

    dbus_error_init(&err);
    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) {
        ERR("failed to connect to DBUS (%s)\n", err.message);
        dbus_error_free(&err);
        sem_post(&sem_control_init);
        return -1;
    }
    if (!conn) {
        ERR("no DBUS connection\n");
        sem_post(&sem_control_init);
        return -1;
    }

    ierr = dbus_bus_request_name(conn, NAMESPACE ".client",
            DBUS_NAME_FLAG_REPLACE_EXISTING | DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);
    if (dbus_error_is_set(&err)) {
        ERR("failed to request DBUS name (%s)\n", err.message);
        dbus_error_free(&err);
        sem_post(&sem_control_init);
        goto out;
    }
    if (ierr != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        ERR("not primary name owner\n");
        sem_post(&sem_control_init);
        goto out;
    }

    msg = dbus_message_new_method_call(NAMESPACE ".server", OBJECT, NAMESPACE ".FitBitD", "Exit");
    if (!msg) {
        ERR("failed to create DBUS message\n");
        goto out;
    }

    if (!dbus_connection_send_with_reply (conn, msg, &pending, -1)) {
        ERR("failed to send message\n");
        goto out;
    }

    if (!pending) {
        ERR("no pending\n");
        goto out;
    }

    dbus_connection_flush(conn);
    dbus_message_unref(msg);
    msg = NULL;

    dbus_pending_call_block(pending);
    msg = dbus_pending_call_steal_reply(pending);
    if (!msg) {
        ERR("no reply\n");
        goto out;
    }
    dbus_pending_call_unref(pending);

    if (!dbus_message_iter_init(msg, &args)) {
        ERR("no reply arguments\n");
        goto out;
    } else if (DBUS_TYPE_UINT32 != dbus_message_iter_get_arg_type(&args)) {
        ERR("invalid reply argument\n");
        goto out;
    } else
        dbus_message_iter_get_basic(&args, &code);

    if (!code) {
        DBG("exit successful\n");
        ret = 0;
    } else {
        ERR("exit failure %u\n", code);
    }

out:
    if (msg)
        dbus_message_unref(msg);
    if (conn)
        dbus_connection_unref(conn);
    return ret;
}
