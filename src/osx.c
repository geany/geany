/*
 *      osx.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2015 Jiri Techet <techet(at)gmail(dot)com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef MAC_INTEGRATION

#include "osx.h"

#include "ui_utils.h"

void osx_ui_init(void)
{
	GtkWidget *item;
	GtkosxApplication *osx_app = gtkosx_application_get();

	item = ui_lookup_widget(main_widgets.window, "menubar1");
	gtk_widget_hide(item);
	gtkosx_application_set_menu_bar(osx_app, GTK_MENU_SHELL(item));

	item = ui_lookup_widget(main_widgets.window, "menu_quit1");
	gtk_widget_hide(item);

	item = ui_lookup_widget(main_widgets.window, "menu_info1");
	gtkosx_application_insert_app_menu_item(osx_app, item, 0);

	item = ui_lookup_widget(main_widgets.window, "menu_help1");
	gtkosx_application_set_help_menu(osx_app, GTK_MENU_ITEM(item));

	gtkosx_application_set_use_quartz_accelerators(osx_app, FALSE);
}

#endif /* MAC_INTEGRATION */

