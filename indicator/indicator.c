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

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libindicator/indicator.h>
#include <libindicator/indicator-object.h>
#include <libindicator/indicator-service-manager.h>

#include "fitbitd.h"

#include "tracker-menuitem.h"

#define INDICATOR_FITBIT_TYPE            (indicator_fitbit_get_type ())
#define INDICATOR_FITBIT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), INDICATOR_FITBIT_TYPE, IndicatorFitbit))
#define INDICATOR_FITBIT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), INDICATOR_FITBIT_TYPE, IndicatorFitbitClass))
#define IS_INDICATOR_FITBIT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), INDICATOR_FITBIT_TYPE))
#define IS_INDICATOR_FITBIT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), INDICATOR_FITBIT_TYPE))
#define INDICATOR_FITBIT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), INDICATOR_FITBIT_TYPE, IndicatorFitbitClass))

typedef struct _IndicatorFitbit         IndicatorFitbit;
typedef struct _IndicatorFitbitClass    IndicatorFitbitClass;
typedef struct _IndicatorFitbitPrivate  IndicatorFitbitPrivate;

struct _IndicatorFitbitClass {
      IndicatorObjectClass parent_class;
};

struct _IndicatorFitbit {
      IndicatorObject parent;
        IndicatorFitbitPrivate *priv;
};

struct _IndicatorFitbitPrivate {
    GtkImage    *image;

    GdkPixbuf   *pixbuf_no_base;
    GdkPixbuf   *pixbuf_idle;
    GdkPixbuf   *pixbuf_syncing;

    GtkMenu     *menu;

    GtkWidget   *account_item;
    GtkWidget   *account_item_label;

    gchar       *accessible_desc;

    gboolean    syncing;
};

#define INDICATOR_FITBIT_GET_PRIVATE(o) \
    (G_TYPE_INSTANCE_GET_PRIVATE ((o), INDICATOR_FITBIT_TYPE, IndicatorFitbitPrivate))

#define INDICATOR_ICON_SIZE 22
#define INDICATOR_ICON_NO_BASE "indicator-fitbit-no-base"
#define INDICATOR_ICON_IDLE    "indicator-fitbit-idle"
#define INDICATOR_ICON_SYNCING "indicator-fitbit-syncing"

GType indicator_fitbit_get_type(void);

/* Indicator Class Functions */
static void indicator_fitbit_class_init(IndicatorFitbitClass *klass);
static void indicator_fitbit_init(IndicatorFitbit *self);
static void indicator_fitbit_dispose(GObject *object);
static void indicator_fitbit_finalize(GObject *object);

/* Indicator Standard Methods */
static GtkImage    *get_image(IndicatorObject *io);
static GtkMenu     *get_menu(IndicatorObject *io);
static const gchar *get_accessible_desc(IndicatorObject *io);
static void         indicator_fitbit_middle_click(IndicatorObject *io, IndicatorObjectEntry *entry,
                                                  guint time, gpointer user_data);

/* callbacks */
static void style_changed_cb(GtkWidget *widget, gpointer user_data);
static void account_item_activated_cb(GtkMenuItem *menuitem, gpointer user_data);
static void trackers_change_cb(void *user);

/* utility functions */
static GdkPixbuf *load_icon(const gchar *name, gint size);
static void attempt_load_pixbuf(GdkPixbuf **pixbuf, const gchar *name, gint size);
static void load_all_pixbufs(IndicatorFitbit *self);
static void refresh_trackers(IndicatorFitbit *self);
static void update_icon(IndicatorFitbit *self);

/* Indicator Module Config */
INDICATOR_SET_VERSION
INDICATOR_SET_TYPE(INDICATOR_FITBIT_TYPE)

G_DEFINE_TYPE (IndicatorFitbit, indicator_fitbit, INDICATOR_OBJECT_TYPE);

static void indicator_fitbit_class_init(IndicatorFitbitClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(IndicatorFitbitPrivate));

    object_class->dispose = indicator_fitbit_dispose;
    object_class->finalize = indicator_fitbit_finalize;

    IndicatorObjectClass *io_class = INDICATOR_OBJECT_CLASS(klass);

    io_class->get_image = get_image;
    io_class->get_menu = get_menu;
    io_class->get_accessible_desc = get_accessible_desc;
    io_class->secondary_activate = indicator_fitbit_middle_click;
}

static void indicator_fitbit_init(IndicatorFitbit *self)
{
    GtkWidget *sep;

    self->priv = INDICATOR_FITBIT_GET_PRIVATE(self);

    /* zero-initialize everything */
    memset(self->priv, 0, sizeof(IndicatorFitbitPrivate));

    self->priv->accessible_desc = _("Fitbit");
    self->priv->menu = GTK_MENU(gtk_menu_new());

    /* create separator */
    sep = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(self->priv->menu), sep);
    gtk_widget_show(sep);

    /* create the account setup menuitem */
    self->priv->account_item_label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(self->priv->account_item_label), 0, 0);
    gtk_label_set_use_markup(GTK_LABEL(self->priv->account_item_label), TRUE);
    gtk_label_set_markup(GTK_LABEL(self->priv->account_item_label), _("Account Setup"));
    gtk_widget_show(self->priv->account_item_label);

    self->priv->account_item = gtk_menu_item_new();
    g_signal_connect(self->priv->account_item, "activate", G_CALLBACK(account_item_activated_cb), self);
    gtk_container_add(GTK_CONTAINER(self->priv->account_item), self->priv->account_item_label);
    gtk_widget_show(self->priv->account_item);

    gtk_menu_shell_append(GTK_MENU_SHELL(self->priv->menu), self->priv->account_item);

    /* find trackers */
    refresh_trackers(self);

    /* stay up to date */
    fitbitd_watch_changes(trackers_change_cb, self);
}

static void indicator_fitbit_dispose(GObject *object)
{
    IndicatorFitbit *self = INDICATOR_FITBIT(object);

    if (self->priv->image) {
        g_object_unref(G_OBJECT(self->priv->image));
        self->priv->image = NULL;
    }

    if (self->priv->pixbuf_no_base) {
        g_object_unref(G_OBJECT(self->priv->pixbuf_no_base));
        self->priv->pixbuf_no_base = NULL;
    }

    if (self->priv->pixbuf_idle) {
        g_object_unref(G_OBJECT(self->priv->pixbuf_idle));
        self->priv->pixbuf_idle = NULL;
    }

    if (self->priv->pixbuf_syncing) {
        g_object_unref(G_OBJECT(self->priv->pixbuf_syncing));
        self->priv->pixbuf_syncing = NULL;
    }

    if (self->priv->menu) {
        g_object_unref(G_OBJECT(self->priv->menu));
        self->priv->menu = NULL;
    }

    G_OBJECT_CLASS(indicator_fitbit_parent_class)->dispose(object);
}

static void indicator_fitbit_finalize(GObject *object)
{
    G_OBJECT_CLASS(indicator_fitbit_parent_class)->finalize(object);
}

static GtkImage *get_image(IndicatorObject *io)
{
    IndicatorFitbit *self = INDICATOR_FITBIT(io);

    if (!self->priv->image) {
        self->priv->image = GTK_IMAGE(gtk_image_new());

        load_all_pixbufs(self);
        update_icon(self);

        g_signal_connect(G_OBJECT(self->priv->image), "style-updated", G_CALLBACK(style_changed_cb), self);
        gtk_widget_show(GTK_WIDGET(self->priv->image));
    }

    return self->priv->image;
}

static GtkMenu *get_menu(IndicatorObject *io)
{
    IndicatorFitbit *self = INDICATOR_FITBIT(io);

    return GTK_MENU(self->priv->menu);
}

static const gchar *get_accessible_desc(IndicatorObject *io)
{
    IndicatorFitbit *self = INDICATOR_FITBIT(io);

    return self->priv->accessible_desc;
}

static void indicator_fitbit_middle_click(IndicatorObject *io, IndicatorObjectEntry *entry,
                                          guint time, gpointer user_data)
{
    //IndicatorFitbit *self = INDICATOR_FITBIT(io);

    /* TODO */
}

static void account_item_activated_cb(GtkMenuItem *menuitem, gpointer user_data)
{
    //IndicatorFitbit *self = INDICATOR_FITBIT(user_data);

    g_return_if_fail(GTK_IS_MENU_ITEM(menuitem));
    g_return_if_fail(IS_INDICATOR_FITBIT(user_data));
}

static void style_changed_cb(GtkWidget *widget, gpointer user_data)
{
    IndicatorFitbit *self = INDICATOR_FITBIT(user_data);

    g_return_if_fail(GTK_IS_IMAGE(widget));
    g_return_if_fail(IS_INDICATOR_FITBIT(user_data));

    load_all_pixbufs(self);
    update_icon(self);
}

static void trackers_change_cb(void *user)
{
    IndicatorFitbit *self = INDICATOR_FITBIT(user);

    refresh_trackers(self);
}

static GdkPixbuf *load_icon(const gchar *name, gint size)
{
    g_return_val_if_fail(name != NULL, NULL);
    g_return_val_if_fail(size > 0, NULL);
    GError *error = NULL;
    GdkPixbuf *pixbuf = NULL;

    /* First try to load the icon from the icon theme */
    GtkIconTheme *theme = gtk_icon_theme_get_default();

    if (gtk_icon_theme_has_icon(theme, name)) {
        pixbuf = gtk_icon_theme_load_icon(theme, name, size, GTK_ICON_LOOKUP_FORCE_SVG, &error);

        if (error) {
            g_warning("Failed to load icon '%s' from icon theme: %s", name, error->message);
        } else {
            return pixbuf;
        }
    }

    /* Otherwise load from the icon installation path */
    gchar *path = g_strdup_printf(ICONS_DIR "/hicolor/scalable/status/%s.svg", name);
    pixbuf = gdk_pixbuf_new_from_file_at_scale(path, size, size, FALSE, &error);

    if (error) {
        g_warning("Failed to load icon at '%s': %s", path, error->message);
        pixbuf = NULL;
    }

    g_free(path);
    return pixbuf;
}

static void attempt_load_pixbuf(GdkPixbuf **pixbuf, const gchar *name, gint size)
{
    GdkPixbuf *loaded;

    loaded = load_icon(name, size);
    if (!loaded) {
        g_warning("failed to load %s icon", name);
        return;
    }

    if (*pixbuf)
        g_object_unref(*pixbuf);

    *pixbuf = loaded;
}

static void load_all_pixbufs(IndicatorFitbit *self)
{
    attempt_load_pixbuf(&self->priv->pixbuf_no_base, INDICATOR_ICON_NO_BASE, INDICATOR_ICON_SIZE);
    attempt_load_pixbuf(&self->priv->pixbuf_idle, INDICATOR_ICON_IDLE, INDICATOR_ICON_SIZE);
    attempt_load_pixbuf(&self->priv->pixbuf_syncing, INDICATOR_ICON_SYNCING, INDICATOR_ICON_SIZE);
}

static void tracker_cb(fitbitd_tracker_t *tracker, void *user)
{
    IndicatorFitbit *self = user;
    GtkWidget *item = tracker->user;

    if (!item) {
        item = tracker_menuitem_new();
        tracker->user = item;
        gtk_widget_show(item);
        gtk_menu_shell_prepend(GTK_MENU_SHELL(self->priv->menu), item);
    }

    self->priv->syncing |= tracker->sync_active;
    update_icon(self);

    tracker_menuitem_set_from_tracker(TRACKER_MENUITEM(item), tracker);
}

static void refresh_trackers(IndicatorFitbit *self)
{
    self->priv->syncing = false;
    fitbitd_enum_trackers(tracker_cb, self);
}

static void update_icon(IndicatorFitbit *self)
{
    if (self->priv->syncing)
        gtk_image_set_from_pixbuf(self->priv->image, self->priv->pixbuf_syncing);
    else
        gtk_image_set_from_pixbuf(self->priv->image, self->priv->pixbuf_idle);
}
