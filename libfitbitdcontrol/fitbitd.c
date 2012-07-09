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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dbus/dbus-glib.h>

#include "fitbitd.h"

#define DBUS_NAMESPACE "eu.paulburton.fitbitd"
#define DBUS_NAMESPACE_PATH "/eu/paulburton/fitbitd"

#define DBUS_SERVICE   DBUS_NAMESPACE ".server"
#define DBUS_PATH      DBUS_NAMESPACE_PATH "/FitBitD"
#define DBUS_INTERFACE DBUS_NAMESPACE ".FitBitD"

#define DBUS_STRUCT_TRACKER (dbus_g_type_get_struct("GValueArray", \
            G_TYPE_STRING, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID))
#define DBUS_ARRAY_TRACKER (dbus_g_type_get_collection("GPtrArray", DBUS_STRUCT_TRACKER))

typedef int (bus_wrappee)(DBusGConnection *conn, DBusGProxy *proxy, void *user);

struct fitbitd_tracker_priv_s {
    fitbitd_tracker_t *next;
};

static fitbitd_tracker_t *trackers = NULL;

static fitbitd_tracker_t *get_tracker(const char *serial)
{
    fitbitd_tracker_t *tr;

    for (tr = trackers; tr; tr = tr->priv->next) {
        if (!strcmp(tr->serial, serial))
            return tr;
    }

    tr = g_new0(fitbitd_tracker_t, 1);
    if (!tr) {
        g_printerr("Failed to alloc fitbitd_tracker_t\n");
        return NULL;
    }

    tr->priv = g_new0(fitbitd_tracker_priv_t, 1);
    if (!tr->priv) {
        g_printerr("Failed to alloc fitbitd_tracker_priv_t\n");
        g_free(tr);
        return NULL;
    }

    strncpy(tr->serial, serial, sizeof(tr->serial));

    /* prepend to trackers list */
    tr->priv->next = trackers;
    trackers = tr;

    return tr;
}

static int bus_wrapper(bus_wrappee *wrappee, void *user, bool cleanup)
{
    DBusGConnection *conn;
    DBusGProxy *proxy;
    GError *err = NULL;
    int ret = -1;

    conn = dbus_g_bus_get(DBUS_BUS_SESSION, &err);
    if (!conn) {
        g_printerr("Failed to connect to session bus: %s\n",
                   err->message);
        goto out;
    }

    proxy = dbus_g_proxy_new_for_name(conn, DBUS_SERVICE, DBUS_PATH, DBUS_INTERFACE);
    if (!proxy) {
        g_printerr("Failed to create bus proxy\n");
        goto out;
    }

    ret = wrappee(conn, proxy, user);

    if (cleanup)
        g_object_unref(proxy);

out:
    if (err)
        g_error_free(err);
    return ret;
}

static int wrappee_exit(DBusGConnection *conn, DBusGProxy *proxy, void *user)
{
    GError *err = NULL;
    guint code;
    int ret = -1;

    if (!dbus_g_proxy_call(proxy, "Exit", &err,
                G_TYPE_INVALID,
                G_TYPE_UINT, &code, G_TYPE_INVALID)) {
        g_printerr("Failed to call Exit\n");
        goto out;
    }

    /* code 0 is success */
    ret = code ? -1 : 0;

out:
    if (err)
        g_error_free(err);
    return ret;
}

int fitbitd_exit(void)
{
    return bus_wrapper(wrappee_exit, NULL, true);
}

typedef struct {
    fitbitd_tracker_callback *callback;
    void *user;
} enum_trackers_state_t;

static void foreach_enum_trackers(gpointer data, gpointer user)
{
    enum_trackers_state_t *state = user;
    GValueArray *str = data;
    fitbitd_tracker_t *tracker;
    GValue *val_serial;
    GValue *val_state;
    GValue *val_sync_age;
    GValue *val_tracker_id;
    GValue *val_user_id;
    guint tracker_state;
    guint sync_age;
    GDateTime *time_now;

    val_serial = g_value_array_get_nth(str, 0);
    val_state = g_value_array_get_nth(str, 1);
    val_sync_age = g_value_array_get_nth(str, 2);
    val_tracker_id = g_value_array_get_nth(str, 3);
    val_user_id = g_value_array_get_nth(str, 4);

    if (!val_serial || !val_state || !val_sync_age || !val_tracker_id || !val_user_id) {
        g_printerr("Missing tracker values\n");
        return;
    }

    tracker = get_tracker(g_value_get_string(val_serial));
    if (!tracker) {
        g_printerr("No tracker record\n");
        return;
    }

    tracker_state = g_value_get_uint(val_state);
    tracker->sync_active = !!(tracker_state & (1 << 0));

    sync_age = g_value_get_uint(val_sync_age);
    time_now = g_date_time_new_now_local();
    if (tracker->sync_time)
        g_date_time_unref(tracker->sync_time);
    tracker->sync_time = g_date_time_add_seconds(time_now, -(gdouble)sync_age);
    g_date_time_unref(time_now);

    strncpy(tracker->id, g_value_get_string(val_tracker_id), sizeof(tracker->id));
    strncpy(tracker->user_id, g_value_get_string(val_user_id), sizeof(tracker->user_id));

    state->callback(tracker, state->user);
}

static void done_enum_trackers(DBusGProxy *proxy, DBusGProxyCall *call, void *user)
{
    GError *err = NULL;
    GPtrArray *array = NULL;
    enum_trackers_state_t *state = user;

    if (!dbus_g_proxy_end_call(proxy, call, &err,
                DBUS_ARRAY_TRACKER, &array, G_TYPE_INVALID)) {
        g_printerr("Failed to call GetDevices: %s\n", err->message);
        goto out;
    }

    g_ptr_array_foreach(array, (GFunc)foreach_enum_trackers, state);

out:
    if (array)
        g_ptr_array_free(array, true);
    if (err)
        g_error_free(err);
    g_object_unref(proxy);
}

static int wrappee_enum_trackers(DBusGConnection *conn, DBusGProxy *proxy, void *user)
{
    enum_trackers_state_t *state = user;

    dbus_g_proxy_begin_call(proxy, "GetDevices", done_enum_trackers, state, g_free, G_TYPE_INVALID);

    return 0;
}

int fitbitd_enum_trackers(fitbitd_tracker_callback *callback, void *user)
{
    enum_trackers_state_t *state;
    int ret;

    state = g_new0(enum_trackers_state_t, 1);
    if (!state) {
        g_printerr("Failed to alloc enum_trackers_state_t\n");
        return -1;
    }

    state->callback = callback;
    state->user = user;

    ret = bus_wrapper(wrappee_enum_trackers, state, false);

    if (ret) {
        /* failure in bus_wrapper is the only way to get here */
        /* if we reached wrappee_enum_trackers then dbus_g_proxy_begin_call will cleanup */
        g_free(state);
    }

    return ret;
}

typedef struct {
    fitbitd_change_callback *callback;
    void *user;
} watch_change_state_t;

static DBusGProxy *signal_proxy = NULL;
static watch_change_state_t *watch_change_state = NULL;

static void callback_signal_proxy_destroyed(DBusGProxy *proxy, gpointer user_data)
{
    signal_proxy = NULL;

    if (watch_change_state) {
        if (fitbitd_watch_changes(watch_change_state->callback, watch_change_state->user))
            g_printerr("Failed to reconnect to fitbitd\n");
    }
}

static void callback_state_changed(DBusGProxy *proxy, void *user)
{
    watch_change_state_t *state = user;

    state->callback(state->user);
}

int fitbitd_watch_changes(fitbitd_change_callback *callback, void *user)
{
    if (!signal_proxy) {
        DBusGConnection *conn;
        GError *err = NULL;

        conn = dbus_g_bus_get(DBUS_BUS_SESSION, &err);
        if (!conn) {
            g_printerr("Failed to connect to session bus: %s\n",
                       err->message);
            return -1;
        }

        signal_proxy = dbus_g_proxy_new_for_name(conn, DBUS_SERVICE, DBUS_PATH, DBUS_INTERFACE);
        if (!signal_proxy) {
            g_printerr("Failed to create bus proxy\n");
            return -1;
        }

        g_signal_connect(signal_proxy, "destroy", G_CALLBACK(callback_signal_proxy_destroyed), NULL); 
        dbus_g_proxy_add_signal(signal_proxy, "StateChanged", G_TYPE_INVALID);
    }

    if (!watch_change_state) {
        watch_change_state = g_new0(watch_change_state_t, 1);
        if (!watch_change_state) {
            g_printerr("Failed to alloc watch_change_state_t\n");
            return -1;
        }
    }

    watch_change_state->callback = callback;
    watch_change_state->user = user;

    dbus_g_proxy_connect_signal(signal_proxy, "StateChanged", (GCallback)callback_state_changed, watch_change_state, NULL);

    return 0;
}
