/*
 *  Copyright (C) 2000, 2001, 2002 Marco Pesenti Gritti
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "prefs-dialog.h"
#include "ephy-dialog.h"
#include "ephy-prefs.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-single.h"
#include "ephy-shell.h"
#include "ephy-gui.h"
#include "eel-gconf-extensions.h"
#include "language-editor.h"
#include "ephy-langs.h"

#include <bonobo/bonobo-i18n.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkradiobutton.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkstock.h>
#include <string.h>

static void
prefs_dialog_class_init (PrefsDialogClass *klass);
static void
prefs_dialog_init (PrefsDialog *pd);
static void
prefs_dialog_finalize (GObject *object);

/* Glade callbacks */
void
prefs_proxy_auto_url_reload_cb (GtkWidget *button,
			        EphyDialog *dialog);
void
prefs_clear_cache_button_clicked_cb (GtkWidget *button,
				     gpointer data);
void
prefs_dialog_response_cb (GtkDialog *dialog, gint response_id, gpointer data);
void
prefs_homepage_current_button_clicked_cb (GtkWidget *button,
					  EphyDialog *dialog);
void
prefs_homepage_blank_button_clicked_cb (GtkWidget *button,
					EphyDialog *dialog);
void
prefs_language_more_button_clicked_cb (GtkWidget *button,
				       EphyDialog *dialog);

static const
struct
{
	char *name;
	char *code;
}
languages [] =
{
	/**
	 * please translate like this: "<your language> (System setting)"
	 * Examples:
	 * "de"    translation: "Deutsch (Systemeinstellung)"
	 * "en_AU" translation: "English, Australia (System setting)" or
	 *                      "Australian English (System setting)"
	 */ 
	{ N_("System language"), "system" },
	{ N_("Afrikaans"), "ak" },
	{ N_("Albanian"), "sq" },
	{ N_("Arabic"), "ar" },
	{ N_("Azerbaijani"), "az" },
	{ N_("Basque"), "eu" },
	{ N_("Breton"), "br" },
	{ N_("Bulgarian"), "bg" },
	{ N_("Byelorussian"), "be" },
	{ N_("Catalan"), "ca" },
	{ N_("Simplified Chinese"), "zh-cn" },
	{ N_("Traditional Chinese"), "zh-tw" },
	{ N_("Chinese"), "zh" },
	{ N_("Croatian"), "hr" },
	{ N_("Czech"), "cs" },
	{ N_("Danish"), "da" },
	{ N_("Dutch"), "nl" },
	{ N_("English"), "en" },
	{ N_("Esperanto"), "eo" },
	{ N_("Estonian"), "et" },
	{ N_("Faeroese"), "fo" },
	{ N_("Finnish"), "fi" },
	{ N_("French"), "fr" },
	{ N_("Galician"), "gl" },
	{ N_("German"), "de" },
	{ N_("Greek"), "el" },
	{ N_("Hebrew"), "he" },
	{ N_("Hungarian"), "hu" },
	{ N_("Icelandic"), "is" },
	{ N_("Indonesian"), "id" },
	{ N_("Irish"), "ga" },
	{ N_("Italian"), "it" },
	{ N_("Japanese"), "ja" },
	{ N_("Korean"), "ko" },
	{ N_("Latvian"), "lv" },
	{ N_("Lithuanian"), "lt" },
	{ N_("Macedonian"), "mk" },
	{ N_("Malay"), "ms" },
	{ N_("Norwegian/Nynorsk"), "nn" },
	{ N_("Norwegian/Bokmal"), "nb" },
	{ N_("Norwegian"), "no" },
	{ N_("Polish"), "pl" },
	{ N_("Portuguese"), "pt" },
	{ N_("Portuguese of Brazil"), "pt-br" },
	{ N_("Romanian"), "ro" },
	{ N_("Russian"), "ru" },
	{ N_("Scottish"), "gd" },
	{ N_("Serbian"), "sr" },
	{ N_("Slovak"), "sk" },
	{ N_("Slovenian"), "sl" },
	{ N_("Spanish"), "es" },
	{ N_("Swedish"), "sv" },
	{ N_("Tamil"), "ta" },
	{ N_("Turkish"), "tr" },
	{ N_("Ukrainian"), "uk" },
	{ N_("Vietnamese"), "vi" },
	{ N_("Walloon"), "wa" }
};
static guint n_languages = G_N_ELEMENTS (languages);

typedef struct
{
	gchar *title;
	gchar *name;
} EncodingAutodetectorInfo;

static EncodingAutodetectorInfo encoding_autodetector[] =
{
	{ N_("Off"),			"" },
	{ N_("Chinese"),		"zh_parallel_state_machine" },
	{ N_("East Asian"),		"cjk_parallel_state_machine" },
	{ N_("Japanese"),		"ja_parallel_state_machine" },
	{ N_("Korean"),			"ko_parallel_state_machine" },
	{ N_("Russian"),		"ruprob" },
	{ N_("Simplified Chinese"),	"zhcn_parallel_state_machine" },
	{ N_("Traditional Chinese"),	"zhtw_parallel_state_machine" },
	{ N_("Universal"),		"universal_charset_detector" },
	{ N_("Ukrainian"),		"ukprob" }
};
static guint n_encoding_autodetectors = G_N_ELEMENTS (encoding_autodetector);

static const
char *cookies_accept_enum [] =
{
	"anywhere", "current site", "nowhere"
};
static guint n_cookies_accept_enum = G_N_ELEMENTS (cookies_accept_enum);

enum
{
	WINDOW_PROP,
	NOTEBOOK_PROP,

	/* General */
	OPEN_IN_TABS_PROP,
	HOMEPAGE_ENTRY_PROP,

	/* Fonts and Colors */
	FIXED_WIDTH_LABEL_PROP,
	VARIABLE_WIDTH_LABEL_PROP,
	MINIMUM_SIZE_LABEL_PROP,
	USE_COLORS_PROP,
	USE_FONTS_PROP,

	/* Privacy */
	ALLOW_POPUPS_PROP,
	ALLOW_JAVA_PROP,
	ALLOW_JS_PROP,
	ACCEPT_COOKIES_PROP,
	DISK_CACHE_PROP,

	/* Language */
	AUTO_ENCODING_PROP,
	DEFAULT_ENCODING_PROP,
	LANGUAGE_PROP,
	LANGUAGE_LABEL_PROP,
	DEFAULT_ENCODING_LABEL_PROP,
	AUTO_ENCODING_LABEL_PROP
};

#define CONF_FONTS_FOR_LANGUAGE	"/apps/epiphany/dialogs/preferences_font_language"

static const
EphyDialogProperty properties [] =
{
	{ WINDOW_PROP, "prefs_dialog", NULL, PT_NORMAL, NULL },
	{ NOTEBOOK_PROP, "prefs_notebook", NULL, PT_NORMAL, NULL },

	/* General */
	{ OPEN_IN_TABS_PROP, "open_in_tabs_checkbutton", CONF_TABS_TABBED, PT_AUTOAPPLY, NULL },
	{ HOMEPAGE_ENTRY_PROP, "homepage_entry", CONF_GENERAL_HOMEPAGE, PT_AUTOAPPLY, NULL },

	/* Fonts and Colors */
	{ FIXED_WIDTH_LABEL_PROP, "fixed_width_label", NULL, PT_NORMAL, NULL },
	{ VARIABLE_WIDTH_LABEL_PROP, "variable_width_label", NULL, PT_NORMAL, NULL },
	{ MINIMUM_SIZE_LABEL_PROP, "minimum_size_label", NULL, PT_NORMAL, NULL },
	{ USE_COLORS_PROP, "use_colors_checkbutton", CONF_RENDERING_USE_OWN_COLORS, PT_AUTOAPPLY, NULL },
	{ USE_FONTS_PROP, "use_fonts_checkbutton", CONF_RENDERING_USE_OWN_FONTS, PT_AUTOAPPLY, NULL },

	/* Privacy */
	{ ALLOW_POPUPS_PROP, "popups_allow_checkbutton", CONF_SECURITY_ALLOW_POPUPS, PT_AUTOAPPLY, NULL },
	{ ALLOW_JAVA_PROP, "enable_java_checkbutton", CONF_SECURITY_JAVA_ENABLED, PT_AUTOAPPLY, NULL },
	{ ALLOW_JS_PROP, "enable_javascript_checkbutton", CONF_SECURITY_JAVASCRIPT_ENABLED, PT_AUTOAPPLY, NULL },
	{ ACCEPT_COOKIES_PROP, "cookies_radiobutton", CONF_SECURITY_COOKIES_ACCEPT, PT_AUTOAPPLY, NULL },
	{ DISK_CACHE_PROP, "disk_cache_spin", CONF_NETWORK_CACHE_SIZE, PT_AUTOAPPLY, NULL },

	/* Languages */
	{ AUTO_ENCODING_PROP, "auto_encoding_optionmenu", NULL, PT_NORMAL, NULL },
	{ DEFAULT_ENCODING_PROP, "default_encoding_optionmenu", NULL, PT_NORMAL, NULL },
	{ LANGUAGE_PROP, "language_optionmenu", NULL, PT_NORMAL, NULL },
	{ LANGUAGE_LABEL_PROP, "language_label", NULL, PT_NORMAL, NULL },
	{ DEFAULT_ENCODING_LABEL_PROP, "default_encoding_label", NULL, PT_NORMAL, NULL },
	{ AUTO_ENCODING_LABEL_PROP, "auto_encoding_label", NULL, PT_NORMAL, NULL },

	{ -1, NULL, NULL }
};

static
int lang_size_group [] =
{
	LANGUAGE_LABEL_PROP,
	DEFAULT_ENCODING_LABEL_PROP,
	AUTO_ENCODING_LABEL_PROP
};
static guint n_lang_size_group = G_N_ELEMENTS (lang_size_group);

static
int font_size_group [] =
{
	FIXED_WIDTH_LABEL_PROP,
	VARIABLE_WIDTH_LABEL_PROP,
	MINIMUM_SIZE_LABEL_PROP
};
static guint n_font_size_group = G_N_ELEMENTS (font_size_group);

typedef struct
{
	gchar *name;
	gchar *key;
	gchar *code;
} EphyLangItem;

#define EPHY_PREFS_DIALOG_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_PREFS_DIALOG, PrefsDialogPrivate))

struct PrefsDialogPrivate
{
	GtkWidget *notebook;
	GtkWidget *window;

	GList *langs;
	GList *encodings;
	GList *autodetectors;
};

static GObjectClass *parent_class = NULL;

GType
prefs_dialog_get_type (void)
{
        static GType prefs_dialog_type = 0;

        if (prefs_dialog_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (PrefsDialogClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) prefs_dialog_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (PrefsDialog),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) prefs_dialog_init
                };

		prefs_dialog_type = g_type_register_static (EPHY_TYPE_DIALOG,
							    "PrefsDialog",
							    &our_info, 0);
        }

        return prefs_dialog_type;

}

EphyDialog *
prefs_dialog_new (GtkWidget *parent)
{
	EphyDialog *dialog;

	dialog = EPHY_DIALOG (g_object_new (EPHY_TYPE_PREFS_DIALOG,
					    "ParentWindow", parent,
					    NULL));

        return dialog;
}

static void
prefs_dialog_class_init (PrefsDialogClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = prefs_dialog_finalize;

	g_type_class_add_private (object_class, sizeof(PrefsDialogPrivate));
}

static void
free_lang_item (EphyLangItem *item, gpointer user_data)
{
	if (item == NULL) return;
		
	g_free (item->name);
	g_free (item->key);
	g_free (item->code);
	g_free (item);
}

static void
prefs_dialog_finalize (GObject *object)
{
	PrefsDialog *pd = EPHY_PREFS_DIALOG (object);

	g_list_foreach (pd->priv->langs, (GFunc) free_lang_item, NULL);
	g_list_free (pd->priv->langs);

	g_list_foreach (pd->priv->encodings, (GFunc) encoding_info_free, NULL);
	g_list_free (pd->priv->encodings);

	g_list_foreach (pd->priv->autodetectors, (GFunc) g_free, NULL);
	g_list_free (pd->priv->autodetectors);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
prefs_dialog_show_help (PrefsDialog *pd)
{
	gint id;

	/* FIXME: Once we actually have documentation we
	 * should point these at the correct links.
	 */
	gchar *help_preferences[] = {
		"setting-preferences",
		"setting-preferences",
		"setting-preferences",
		"setting-preferences"
	};

	id = gtk_notebook_get_current_page (GTK_NOTEBOOK (pd->priv->notebook));

	ephy_gui_help (GTK_WINDOW (pd->priv->window), "epiphany", help_preferences[id]);
}

static void
default_encoding_menu_changed_cb (GtkOptionMenu *option_menu,
				  PrefsDialog *dialog)
{
	GList *encoding;
	gint i;
	EncodingInfo *info;

	i = gtk_option_menu_get_history (option_menu);
	encoding = g_list_nth (dialog->priv->encodings, i);
	g_assert (encoding != NULL);

	info = (EncodingInfo *) encoding->data;
	eel_gconf_set_string (CONF_LANGUAGE_DEFAULT_ENCODING, info->encoding);
}

static gint
find_encoding_in_list_cmp (const EncodingInfo *info, const gchar *encoding)
{
	return strcmp (info->encoding, encoding);
}

static void
create_default_encoding_menu (PrefsDialog *dialog)
{
	GList *l;
	GtkWidget *menu, *optionmenu;
	gchar *encoding;
	EphyEmbedSingle *single;

	single = ephy_embed_shell_get_embed_single
		(EPHY_EMBED_SHELL (ephy_shell));

	ephy_embed_single_get_encodings (single, LG_ALL, TRUE,
					 &dialog->priv->encodings);

	menu = gtk_menu_new ();

	optionmenu = ephy_dialog_get_control (EPHY_DIALOG (dialog),
					      DEFAULT_ENCODING_PROP);

	for (l = dialog->priv->encodings; l != NULL; l = l->next)
	{
		EncodingInfo *info = (EncodingInfo *) l->data;
		GtkWidget *item;

		item = gtk_menu_item_new_with_label (info->title);
		gtk_menu_shell_append (GTK_MENU_SHELL(menu), item);
		gtk_widget_show (item);
	}

	gtk_option_menu_set_menu (GTK_OPTION_MENU(optionmenu), menu);

	/* init value */
	encoding = eel_gconf_get_string (CONF_LANGUAGE_DEFAULT_ENCODING);
	/* fallback */
	if (encoding == NULL) encoding = g_strdup ("ISO-8859-1");

	l = g_list_find_custom (dialog->priv->encodings, encoding,
				(GCompareFunc) find_encoding_in_list_cmp);
	gtk_option_menu_set_history (GTK_OPTION_MENU(optionmenu),
				     g_list_position (dialog->priv->encodings, l));
	g_free (encoding);

	g_signal_connect (optionmenu, "changed",
			  G_CALLBACK (default_encoding_menu_changed_cb),
			  dialog);
}

static void
autodetect_encoding_menu_changed_cb (GtkOptionMenu *option_menu, gpointer data)
{
	GList *l;
	guint i;
	EncodingAutodetectorInfo *info;

	g_return_if_fail (EPHY_IS_PREFS_DIALOG (data));

	i = gtk_option_menu_get_history (option_menu);

	l = g_list_nth (EPHY_PREFS_DIALOG (data)->priv->autodetectors, i);

	if (l)
	{
		info = (EncodingAutodetectorInfo *) l->data;

		eel_gconf_set_string (CONF_LANGUAGE_AUTODETECT_ENCODING, info->name);
	}
}

static gint
autodetector_info_cmp (const EncodingAutodetectorInfo *i1, const EncodingAutodetectorInfo *i2)
{
	return g_utf8_collate (i1->title, i2->title);
}

static gint
find_autodetector_info (const EncodingAutodetectorInfo *info, const gchar *name)
{
	return strcmp (info->name, name);
}

static void
create_encoding_autodetectors_menu (PrefsDialog *dialog)
{
	GtkWidget *optionmenu, *menu, *item;
	gint i, position = 0;
	GList *l, *list = NULL;
	gchar *detector = NULL;
	EncodingAutodetectorInfo *info;

	optionmenu = ephy_dialog_get_control (EPHY_DIALOG (dialog),
					      AUTO_ENCODING_PROP);

	for (i = 0; i < n_encoding_autodetectors; i++)
	{
		info = g_new0 (EncodingAutodetectorInfo, 1);

		info->title = _(encoding_autodetector[i].title);
		info->name = encoding_autodetector[i].name;

		list = g_list_prepend (list, info);
	}

	list = g_list_sort (list, (GCompareFunc) autodetector_info_cmp);
	dialog->priv->autodetectors = list;

	menu = gtk_menu_new ();

	for (l = list; l != NULL; l = l->next)
	{
		info = (EncodingAutodetectorInfo *) l->data;

		item = gtk_menu_item_new_with_label (info->title);
		gtk_menu_shell_append (GTK_MENU_SHELL(menu), item);
		gtk_widget_show (item);
		g_object_set_data (G_OBJECT (item), "desc", info->title);
	}

	gtk_option_menu_set_menu (GTK_OPTION_MENU (optionmenu), menu);

	/* init value */
	detector = eel_gconf_get_string (CONF_LANGUAGE_AUTODETECT_ENCODING);
	if (detector == NULL) detector = g_strdup ("");

	l = g_list_find_custom (list, detector,
				(GCompareFunc) find_autodetector_info);

	g_free (detector);

	if (l)
	{
		position = g_list_position (list, l);
	}

	gtk_option_menu_set_history (GTK_OPTION_MENU(optionmenu), position);

	g_signal_connect (optionmenu, "changed",
			  G_CALLBACK (autodetect_encoding_menu_changed_cb),
			  dialog);
}

static gint
compare_lang_items (const EphyLangItem *i1, const EphyLangItem *i2)
{
	return strcmp (i1->key, i2->key);
}

static gint
find_lang_code (const EphyLangItem *i1, const gchar *code)
{
	return strcmp (i1->code, code);
}

static void
create_languages_list (PrefsDialog *dialog)
{
	GList *list = NULL, *lang;
	GSList *pref_list, *l;
	EphyLangItem *item;
	const gchar *code;
	guint i;

	for (i = 0; i < n_languages; i++)
	{		
		item = g_new0 (EphyLangItem, 1);

		item->name = g_strdup (_(languages[i].name));
		item->key  = g_utf8_collate_key (item->name, -1);
		item->code = g_strdup (languages[i].code);

		list = g_list_prepend (list, item);
	}

	/* add custom languages */
	pref_list = eel_gconf_get_string_list (CONF_RENDERING_LANGUAGE);

	for (l = pref_list; l != NULL; l = l->next)
	{
		code = (const gchar*) l->data;

		lang = g_list_find_custom (list, code,
					   (GCompareFunc) find_lang_code);

		if (lang == NULL)
		{
			/* not found in list */
			item = g_new0 (EphyLangItem, 1);

			item->name = g_strdup_printf (_("Custom [%s]"), code);
			item->key  = g_utf8_collate_key (item->name, -1);
			item->code = g_strdup (code);

			list = g_list_prepend (list, item);
		}
	}

	if (pref_list)
	{
		g_slist_foreach (pref_list, (GFunc) g_free, NULL);
		g_slist_free (pref_list);
	}

	list = g_list_sort (list, (GCompareFunc) compare_lang_items);

	dialog->priv->langs = list;
}

static GtkWidget *
general_prefs_new_language_menu (PrefsDialog *dialog)
{
	GList *l;
	GtkWidget *menu;
	EphyLangItem *li;

	menu = gtk_menu_new ();

	for (l = dialog->priv->langs; l != NULL; l = l->next)
	{
		GtkWidget *item;

		li = (EphyLangItem*) l->data;
		item = gtk_menu_item_new_with_label (li->name);
		gtk_menu_shell_append (GTK_MENU_SHELL(menu), item);
		gtk_widget_show (item);
		g_object_set_data (G_OBJECT (item), "desc", li->name);
	}

	return menu;
}

static void
language_menu_changed_cb (GtkOptionMenu *option_menu,
		          gpointer data)
{
	gint i;
	GSList *list = NULL;
	GList *lang = NULL;

	g_return_if_fail (EPHY_IS_PREFS_DIALOG (data));

	list = eel_gconf_get_string_list (CONF_RENDERING_LANGUAGE);
	g_return_if_fail (list != NULL);

	/* Subst the first item according to the optionmenu */
	i = gtk_option_menu_get_history (option_menu);

	lang = g_list_nth (EPHY_PREFS_DIALOG (data)->priv->langs, i);

	if (lang)
	{
		g_free (list->data);
		list->data = g_strdup (((EphyLangItem *) lang->data)->code);

		eel_gconf_set_string_list (CONF_RENDERING_LANGUAGE, list);
	}

	g_slist_foreach (list, (GFunc) g_free, NULL);
	g_slist_free (list);
}

static void
create_language_menu (PrefsDialog *dialog)
{
	GtkWidget *optionmenu;
	GtkWidget *menu;
	const gchar *code;
	gint i = 0;
	GSList *list;
	GList *lang;

	optionmenu = ephy_dialog_get_control (EPHY_DIALOG (dialog),
					      LANGUAGE_PROP);

	menu = general_prefs_new_language_menu (dialog);

	gtk_option_menu_set_menu (GTK_OPTION_MENU(optionmenu), menu);

	/* init value from first element of the list */
	list = eel_gconf_get_string_list (CONF_RENDERING_LANGUAGE);
	if (list)
	{
		code = (const gchar *) list->data;
		lang = g_list_find_custom (dialog->priv->langs, code,
					   (GCompareFunc)find_lang_code);

		if (lang)
		{
			i = g_list_position (dialog->priv->langs, lang);
		}

		g_slist_foreach (list, (GFunc) g_free, NULL);
		g_slist_free (list);
	}

	gtk_option_menu_set_history (GTK_OPTION_MENU(optionmenu), i);

	g_signal_connect (optionmenu, "changed",
			  G_CALLBACK (language_menu_changed_cb),
			  dialog);
}

static void
set_homepage_entry (EphyDialog *dialog,
		    const char *new_location)
{
	GtkWidget *entry;
	int pos = 0;

	entry = ephy_dialog_get_control (dialog, HOMEPAGE_ENTRY_PROP);

	gtk_editable_delete_text (GTK_EDITABLE (entry), 0, -1);
	gtk_editable_insert_text (GTK_EDITABLE (entry), new_location,
				  strlen (new_location),
				  &pos);
}


static void
prefs_dialog_init (PrefsDialog *pd)
{
	EphyDialog *dialog = EPHY_DIALOG (pd);
	GdkPixbuf *icon;

	pd->priv = EPHY_PREFS_DIALOG_GET_PRIVATE (pd);

	ephy_dialog_construct (EPHY_DIALOG (pd),
			       properties,
			       "prefs-dialog.glade",
			       "prefs_dialog");

	ephy_dialog_add_enum (EPHY_DIALOG (pd), ACCEPT_COOKIES_PROP,
			      n_cookies_accept_enum, cookies_accept_enum);

	ephy_dialog_set_size_group (EPHY_DIALOG (pd), lang_size_group,
				    n_lang_size_group);
	ephy_dialog_set_size_group (EPHY_DIALOG (pd), font_size_group,
				    n_font_size_group);

	pd->priv->window = ephy_dialog_get_control (dialog, WINDOW_PROP);
	pd->priv->notebook = ephy_dialog_get_control (dialog, NOTEBOOK_PROP);
	pd->priv->encodings = NULL;
	pd->priv->autodetectors = NULL;

	icon = gtk_widget_render_icon (pd->priv->window,
				       GTK_STOCK_PREFERENCES,
				       GTK_ICON_SIZE_MENU,
				       "prefs_dialog");
	gtk_window_set_icon (GTK_WINDOW (pd->priv->window), icon);
	g_object_unref(icon);

	create_languages_list (pd);
	create_default_encoding_menu (pd);
	create_encoding_autodetectors_menu (pd);
	create_language_menu (pd);
}

void
prefs_dialog_response_cb (GtkDialog *dialog, gint response_id, gpointer data)
{
	if (response_id == GTK_RESPONSE_CLOSE)
	{
		gtk_widget_destroy (GTK_WIDGET(dialog));
	}
	else if (response_id == GTK_RESPONSE_HELP)
	{
		PrefsDialog *pd = (PrefsDialog *)data;
		prefs_dialog_show_help (pd);
	}
}

void
prefs_clear_cache_button_clicked_cb (GtkWidget *button,
				     gpointer data)
{
	EphyEmbedSingle *single;

	single = ephy_embed_shell_get_embed_single
		(EPHY_EMBED_SHELL (ephy_shell));

	ephy_embed_single_clear_cache (single);
}

void
prefs_homepage_current_button_clicked_cb (GtkWidget *button,
					  EphyDialog *dialog)
{
	EphyWindow *window;
	EphyEmbed *embed;
	char *location;

	window = ephy_shell_get_active_window (ephy_shell);
	g_return_if_fail (window != NULL);

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_get_location (embed, TRUE, &location);
	set_homepage_entry (dialog, location);
	g_free (location);
}

void
prefs_homepage_blank_button_clicked_cb (GtkWidget *button,
					EphyDialog *dialog)
{
	set_homepage_entry (dialog, "");
}

static void
fill_language_editor (LanguageEditor *le, PrefsDialog *dialog)
{
	GSList *strings;
	GSList *tmp;
	GList *lang;
	gint i;
	const gchar *code;
	EphyLangItem *li;

	strings = eel_gconf_get_string_list (CONF_RENDERING_LANGUAGE);
	g_return_if_fail (strings != NULL);

	for (tmp = strings; tmp != NULL; tmp = g_slist_next (tmp))
	{
		code = (const gchar *) tmp->data;

		lang = g_list_find_custom (dialog->priv->langs, code,
					   (GCompareFunc) find_lang_code);

		if (lang)
		{
			i = g_list_position (dialog->priv->langs, lang);
			li = (EphyLangItem *) lang->data;

			language_editor_add (le, li->name, i);
		}
	}

	g_slist_foreach (strings, (GFunc) g_free, NULL);
	g_slist_free (strings);
}

static void
language_dialog_changed_cb (LanguageEditor *le,
			    GSList *list,
			    PrefsDialog *dialog)
{
	GtkWidget *optionmenu;
	const GSList *l;
	GSList *langs = NULL;
	GList *lang;
	gint i;
	EphyLangItem *li;

	optionmenu = ephy_dialog_get_control (EPHY_DIALOG (dialog),
						LANGUAGE_PROP);
	gtk_option_menu_set_history (GTK_OPTION_MENU(optionmenu),
				     GPOINTER_TO_INT(list->data));

	for (l = list; l != NULL; l = l->next)
	{
		i = GPOINTER_TO_INT (l->data);
		lang = g_list_nth (dialog->priv->langs, i);

		if (lang)
		{
			li = (EphyLangItem *) lang->data;

			langs = g_slist_append (langs, li->code);
		}
	}

	eel_gconf_set_string_list (CONF_RENDERING_LANGUAGE, langs);
	g_slist_free (langs);
}

void
prefs_language_more_button_clicked_cb (GtkWidget *button,
				       EphyDialog *dialog)
{
	LanguageEditor *editor;
	GtkWidget *menu;
	GtkWidget *toplevel;

	menu = general_prefs_new_language_menu (EPHY_PREFS_DIALOG (dialog));

	toplevel = gtk_widget_get_toplevel (button);
	editor = language_editor_new (toplevel);
	language_editor_set_menu (editor, menu);
	fill_language_editor (editor, EPHY_PREFS_DIALOG (dialog));
	ephy_dialog_set_modal (EPHY_DIALOG(editor), TRUE);

	g_signal_connect (editor, "changed",
			  G_CALLBACK(language_dialog_changed_cb),
			  dialog);

	ephy_dialog_show (EPHY_DIALOG(editor));
}
