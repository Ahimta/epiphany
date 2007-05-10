/*
 * Copyright © 2005, 2006 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  $Id$
 */

#include <glib.h>
#include <gtk/gtk.h>

#define COMPILING_TESTSPINNER
#define LOG(msg, args...) g_print(msg, ## args); g_print ("\n")
#define START_PROFILER(name)
#define STOP_PROFILER(name)

#include "ephy-spinner.c"
#include "ephy-spinner-tool-item.c"

#define MOVE_TIMEOUT	211 /* ms */

static void start_or_stop (GtkToggleButton *button, EphySpinner *spinner)
{
	if (gtk_toggle_button_get_active (button))
	{
		ephy_spinner_start (spinner);
	}
	else
	{
		ephy_spinner_stop (spinner);
	}
}

static void add_spinner (GtkTable *table,
			 int row,
			 GtkIconSize size,
			 const char *sizename,
			 guint interval,
			 gboolean start)
{
	GtkWidget *label, *frame, *button, *spinner;
	char *text;

	text = g_strdup_printf ("%s size:", sizename);
	label = gtk_label_new (text);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_table_attach_defaults (table, label, 0, 1, row, row + 1);
	g_free (text);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_table_attach (table, frame, 1, 2, row, row + 1, GTK_SHRINK, GTK_SHRINK, 0, 0);

	spinner = ephy_spinner_new ();
	ephy_spinner_set_size (EPHY_SPINNER (spinner), size);
	gtk_container_add (GTK_CONTAINER (frame), spinner);

	button = gtk_check_button_new_with_label ("Spin");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), start);
	start_or_stop (GTK_TOGGLE_BUTTON (button), EPHY_SPINNER (spinner));
	g_signal_connect (button, "toggled", G_CALLBACK (start_or_stop), spinner);
	gtk_table_attach_defaults (table, button, 2, 3, row, row + 1);
}

static void move_window (GtkWindow *window)
{
	GdkScreen *screen = gtk_widget_get_screen (GTK_WIDGET (window));
	GdkDisplay *display = gdk_screen_get_display (screen);
	gint number_of_screens = gdk_display_get_n_screens (display);
	gint screen_num = gdk_screen_get_number (screen);
	
	if ((screen_num + 1) < number_of_screens)
	{
		gtk_window_set_screen (window, gdk_display_get_screen (display, screen_num + 1));
	}
	else
	{
		gtk_window_set_screen (window, gdk_display_get_screen (display, 0));
	}
}

static gboolean
move_true (GtkWindow *window)
{
	move_window (window);
	return TRUE;
}

static void
start_or_stop_repeated_moves (GtkWindow *window)
{
	static guint timeout = 0;

	if (timeout == 0)
	{
		timeout = g_timeout_add (MOVE_TIMEOUT, (GSourceFunc) move_true, window);
	}
	else
	{
		g_source_remove (timeout);
		timeout = 0;
	}
}

static void
change_toolbar_style_cb (GtkComboBox *combo,
			 GtkToolbar *toolbar)
{
	int value = gtk_combo_box_get_active (combo);

	gtk_toolbar_set_style (toolbar, value);
}

static void
change_toolbar_icon_size_cb (GtkComboBox *combo,
			     GtkToolbar *toolbar)
{
	int value = gtk_combo_box_get_active (combo);

	if (value == GTK_ICON_SIZE_INVALID)
	{
		gtk_toolbar_unset_icon_size (toolbar);
	}
	else
	{
		gtk_toolbar_set_icon_size (toolbar, value);
	}
}

static void
spin_toolbar_spinner_cb (GtkToggleButton *button,
			 EphySpinnerToolItem *item)
{
	ephy_spinner_tool_item_set_spinning (item, gtk_toggle_button_get_active (button));
}

int main(int argc, char **argv)
{
	GtkWidget *window, *vbox, *vbox2, *widget, *toolbar, *combo;
	GtkToolItem *item;
	GtkTable *table;
	int row = 0, i;
	const char *toolbar_styles[] = {
		"icons",
		"text",
		"both (vertical)",
		"both (horizontal)"
	};
	const char *icon_sizes[] = {
		"default",
		"menu",
		"small toolbar",
		"large toolbar",
		"button",
		"dnd",
		"dialog"
	};
	
	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width (GTK_CONTAINER (window), 12);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	widget = gtk_table_new (5, 3, FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

	table = GTK_TABLE (widget);
	gtk_table_set_row_spacings (table, 6);
	gtk_table_set_col_spacings (table,12);

	add_spinner (table, row++, GTK_ICON_SIZE_INVALID, "Native", 0, FALSE);
	add_spinner (table, row++, GTK_ICON_SIZE_MENU, "Menu", 0, FALSE);
	add_spinner (table, row++, GTK_ICON_SIZE_SMALL_TOOLBAR, "Small toolbar", 0, FALSE);
	add_spinner (table, row++, GTK_ICON_SIZE_LARGE_TOOLBAR, "Large toolbar", 0, FALSE);
	add_spinner (table, row++, GTK_ICON_SIZE_BUTTON, "Button", 0, FALSE);
	add_spinner (table, row++, GTK_ICON_SIZE_DND, "Drag-and-drop", 0, FALSE);
	add_spinner (table, row++, GTK_ICON_SIZE_DIALOG, "Dialog", 0, FALSE);

	/* Test toolbar */
	vbox2 = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), vbox2, FALSE, FALSE, 0);

	toolbar = gtk_toolbar_new ();
	gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH);
	gtk_box_pack_end (GTK_BOX (vbox2), toolbar, FALSE, FALSE, 0);

	item = gtk_tool_button_new_from_stock (GTK_STOCK_NEW);
	gtk_tool_item_set_homogeneous (item, FALSE);
	gtk_tool_item_set_is_important (item, TRUE);
	gtk_widget_show (GTK_WIDGET (item));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, 0);

	item = gtk_tool_button_new_from_stock (GTK_STOCK_OPEN);
	gtk_tool_item_set_homogeneous (item, FALSE);
	gtk_widget_show (GTK_WIDGET (item));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, 0);

	item = gtk_tool_item_new ();
	gtk_tool_item_set_homogeneous (item, FALSE);
	gtk_widget_show (GTK_WIDGET (item));
	gtk_tool_item_set_expand (item, TRUE);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

	item = ephy_spinner_tool_item_new ();
	gtk_widget_show (GTK_WIDGET (item));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

	widget = gtk_check_button_new_with_label ("Spin");
	g_signal_connect (widget, "toggled", G_CALLBACK (spin_toolbar_spinner_cb), item);
	gtk_box_pack_start (GTK_BOX (vbox2), widget, FALSE, FALSE, 0);

	combo = gtk_combo_box_new_text ();
	g_signal_connect (combo, "changed", G_CALLBACK (change_toolbar_style_cb), toolbar);
	for (i = 0; i < G_N_ELEMENTS (toolbar_styles); ++i)
	{
		gtk_combo_box_append_text (GTK_COMBO_BOX (combo), toolbar_styles[i]);
	}
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), GTK_TOOLBAR_BOTH);
	gtk_box_pack_start (GTK_BOX (vbox2), combo, FALSE, FALSE, 0);

	combo = gtk_combo_box_new_text ();
	g_signal_connect (combo, "changed", G_CALLBACK (change_toolbar_icon_size_cb), toolbar);
	for (i = 0; i < G_N_ELEMENTS (icon_sizes); ++i)
	{
		gtk_combo_box_append_text (GTK_COMBO_BOX (combo), icon_sizes[i]);
	}
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), GTK_ICON_SIZE_INVALID);
	gtk_box_pack_start (GTK_BOX (vbox2), combo, FALSE, FALSE, 0);

	/* Controls */
	widget = gtk_button_new_with_label ("Move to next screen");
	g_signal_connect_swapped (widget, "clicked", G_CALLBACK (move_window), window);
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

	widget = gtk_toggle_button_new_with_label ("Move repeatedly to next screen");
	g_signal_connect_swapped (widget, "toggled", G_CALLBACK (start_or_stop_repeated_moves), window);
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

	widget = gtk_button_new_with_label ("Quit");
	g_signal_connect_swapped (widget, "clicked", G_CALLBACK (gtk_widget_destroy), window);
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

	gtk_widget_show_all (window);

	g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

	gtk_main ();

	return 0;
}
