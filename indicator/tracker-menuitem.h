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

#ifndef __tracker_menuitem_h__
#define __tracker_menuitem_h__

#include <gtk/gtk.h>
#include "fitbitd.h"

G_BEGIN_DECLS

#define TRACKER_MENUITEM_TYPE             (tracker_menuitem_get_type ())
#define TRACKER_MENUITEM(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), TRACKER_MENUITEM_TYPE, TrackerMenuItem))
#define TRACKER_MENUITEM_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_MENUITEM_TYPE, TrackerMenuItemClass))
#define IS_TRACKER_MENUITEM(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TRACKER_MENUITEM_TYPE))
#define IS_TRACKER_MENUITEM_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_MENUITEM_TYPE))

typedef struct _TrackerMenuItem        TrackerMenuItem;
typedef struct _TrackerMenuItemClass   TrackerMenuItemClass;
typedef struct _TrackerMenuItemPrivate TrackerMenuItemPrivate;

struct _TrackerMenuItem
{
    GtkMenuItem parent_instance;
    TrackerMenuItemPrivate *priv;
};

struct _TrackerMenuItemClass
{
    GtkMenuItemClass parent_class;
    void (* clicked)(TrackerMenuItem *menuitem);
};

#define TRACKER_MENUITEM_GET_PRIVATE(o) \
	  (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_MENUITEM_TYPE, TrackerMenuItemPrivate))

#define TRACKER_MENUITEM_SIGNAL_CLICKED "clicked"

GType      tracker_menuitem_get_type(void);
GtkWidget *tracker_menuitem_new(void);
void       tracker_menuitem_set_time_from_tracker(TrackerMenuItem *self, fitbitd_tracker_t *tracker);
void       tracker_menuitem_set_from_tracker(TrackerMenuItem *self, fitbitd_tracker_t *tracker);

G_END_DECLS

#endif /* __tracker_menuitem_h__ */
