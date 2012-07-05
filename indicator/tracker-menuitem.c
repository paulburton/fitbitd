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

#define GETTEXT_PACKAGE "indicator-fitbit"

#include <glib/gi18n-lib.h>
#include "tracker-menuitem.h"

#define TRACKER_MENUITEM_MAX_CHARS 42

enum {
  CLICKED,
  LAST_SIGNAL
};

struct _TrackerMenuItemPrivate {
    GtkWidget *label_user;
    GtkWidget *label_time;
};

static void tracker_menuitem_class_init(TrackerMenuItemClass *klass);
static void tracker_menuitem_init(TrackerMenuItem *self);

static void     tracker_activated_cb(GtkMenuItem *menuitem, gpointer user_data);
static gboolean tracker_button_press_cb(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean tracker_button_release_cb(GtkWidget *widget, GdkEventButton *event, gpointer user_data);

static guint tracker_menuitem_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(TrackerMenuItem, tracker_menuitem, GTK_TYPE_MENU_ITEM);

static void tracker_menuitem_class_init(TrackerMenuItemClass *klass)
{
    GtkMenuItemClass *menu_item_class = GTK_MENU_ITEM_CLASS(klass);

    g_type_class_add_private(klass, sizeof(TrackerMenuItemPrivate));

    menu_item_class->hide_on_activate = FALSE;

    tracker_menuitem_signals[CLICKED] =
        g_signal_new(TRACKER_MENUITEM_SIGNAL_CLICKED,
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(TrackerMenuItemClass, clicked),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
}

static void tracker_menuitem_init(TrackerMenuItem *self)
{
    GtkWidget *hbox, *vbox;

    self->priv = TRACKER_MENUITEM_GET_PRIVATE(self);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    /* info vbox */
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    /* user label */
    self->priv->label_user = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(self->priv->label_user), 0, 0);
    gtk_label_set_use_markup(GTK_LABEL(self->priv->label_user), TRUE);
    gtk_label_set_line_wrap(GTK_LABEL(self->priv->label_user), TRUE);
    gtk_label_set_line_wrap_mode(GTK_LABEL(self->priv->label_user), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_max_width_chars(GTK_LABEL(self->priv->label_user), TRACKER_MENUITEM_MAX_CHARS);
    gtk_box_pack_start(GTK_BOX(vbox), self->priv->label_user, TRUE, TRUE, 0);
    gtk_widget_show(self->priv->label_user);

    /* time label */
    self->priv->label_time = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(self->priv->label_time), 0, 0);
    gtk_label_set_use_markup(GTK_LABEL(self->priv->label_time), TRUE);
    gtk_label_set_line_wrap(GTK_LABEL(self->priv->label_time), FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), self->priv->label_time, TRUE, TRUE, 0);
    gtk_widget_show(self->priv->label_time);

    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
    gtk_widget_show(vbox);

    gtk_container_add(GTK_CONTAINER(self), hbox);
    gtk_widget_show(hbox);

    g_signal_connect(self, "activate", G_CALLBACK(tracker_activated_cb), NULL);
    g_signal_connect(self, "button-press-event", G_CALLBACK(tracker_button_press_cb), NULL);
    g_signal_connect(self, "button-release-event", G_CALLBACK(tracker_button_release_cb), NULL);
}

GtkWidget *tracker_menuitem_new(void)
{
    return g_object_new(TRACKER_MENUITEM_TYPE, NULL);
}

void tracker_menuitem_set_time_from_tracker(TrackerMenuItem *self, fitbitd_tracker_t *tracker)
{
    gchar *time_str, *markup;

    if (tracker->sync_active) {
        gtk_label_set_markup(GTK_LABEL(self->priv->label_time), "<small>Synchronising...</small>");
        return;
    }

    time_str = g_date_time_format(tracker->sync_time, "%R");
    if (time_str) {
        markup = g_strdup_printf("<small>Last synced at %s</small>", time_str);

        if (markup) {
            gtk_label_set_markup(GTK_LABEL(self->priv->label_time), markup);
            g_free(markup);
        }
    }
}

void tracker_menuitem_set_from_tracker(TrackerMenuItem *self, fitbitd_tracker_t *tracker)
{
    gchar *username, *markup;

    if (tracker->user_id[0])
        username = tracker->user_id;
    else
        username = "Unlinked Tracker";

    markup = g_strdup_printf("<b>%s</b>", username);

    if (!markup) {
        g_printerr("Failed to alloc markup\n");
        return;
    }

    gtk_label_set_markup(GTK_LABEL(self->priv->label_user), markup);
    g_free(markup);

    tracker_menuitem_set_time_from_tracker(self, tracker);
}

static void tracker_activated_cb(GtkMenuItem *menuitem, gpointer user_data)
{
    g_return_if_fail(IS_TRACKER_MENUITEM(menuitem));

    g_signal_emit(TRACKER_MENUITEM(menuitem), tracker_menuitem_signals[CLICKED], 0);
}

static gboolean tracker_button_press_cb(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    return true;
}

static gboolean tracker_button_release_cb(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    g_return_val_if_fail(IS_TRACKER_MENUITEM(widget), FALSE);

    g_signal_emit(TRACKER_MENUITEM(widget), tracker_menuitem_signals[CLICKED], 0);

    return true;
}
