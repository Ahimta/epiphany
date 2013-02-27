/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
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
 */

#include "config.h"
#include "ephy-embed-single.h"

#include "ephy-about-handler.h"
#include "ephy-debug.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-type-builtins.h"
#include "ephy-file-helpers.h"
#include "ephy-prefs.h"
#include "ephy-profile-utils.h"
#include "ephy-request-about.h"
#include "ephy-settings.h"
#include "ephy-signal-accumulator.h"
#include "ephy-string.h"

#include <glib/gi18n.h>
#include <libsoup/soup.h>
#ifdef HAVE_WEBKIT2
#include <webkit2/webkit2.h>
#else
#include <webkit/webkit.h>
#endif

#define EPHY_EMBED_SINGLE_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EMBED_SINGLE, EphyEmbedSinglePrivate))

#define NSPLUGINWRAPPER_SETUP "/usr/bin/mozilla-plugin-config"

struct _EphyEmbedSinglePrivate {
  GHashTable *form_auth_data;
#ifndef HAVE_WEBKIT2
  SoupCache *cache;
#endif
};

G_DEFINE_TYPE (EphyEmbedSingle, ephy_embed_single, G_TYPE_OBJECT)

static void
form_auth_data_free (EphyEmbedSingleFormAuthData *data)
{
  g_free (data->form_username);
  g_free (data->form_password);
  g_free (data->username);

  g_slice_free (EphyEmbedSingleFormAuthData, data);
}

static EphyEmbedSingleFormAuthData*
form_auth_data_new (const char *form_username,
                    const char *form_password,
                    const char *username)
{
  EphyEmbedSingleFormAuthData *data;

  data = g_slice_new (EphyEmbedSingleFormAuthData);
  data->form_username = g_strdup (form_username);
  data->form_password = g_strdup (form_password);
  data->username = g_strdup (username);

  return data;
}

static void
store_form_data_cb (SecretService *service, GAsyncResult *result, EphyEmbedSingle *single)
{
  GList *results, *p;
  SecretItem *item;
  GHashTable *attributes;
  char *host, *form_username, *form_password, *username;

  GError *error = NULL;

  results = secret_service_search_finish (service, result, &error);
  if (error != NULL) {
    g_warning ("Error caching form data: %s", error->message);
    g_error_free (error);
    return;
  }

  for (p = results; p; p = p->next) {
    item = (SecretItem *)p->data;
    attributes = secret_item_get_attributes (item);
    host = ephy_string_get_host_name (g_hash_table_lookup (attributes, "uri"));
    form_username = g_hash_table_lookup (attributes, FORM_USERNAME_KEY);
    form_password = g_hash_table_lookup (attributes, FORM_PASSWORD_KEY);
    username = g_hash_table_lookup (attributes, "username");

    ephy_embed_single_add_form_auth (single, host, form_username, form_password, username);

    g_free (host);
    g_hash_table_unref (attributes);
  }
  g_list_free_full (results, (GDestroyNotify)g_object_unref);
}

static void
cache_secret_form_data (EphyEmbedSingle *single)
{
  GHashTable *attributes;

  attributes = secret_attributes_build (EPHY_FORM_PASSWORD_SCHEMA, NULL);
  secret_service_search (NULL,
                         EPHY_FORM_PASSWORD_SCHEMA,
                         attributes,
                         SECRET_SEARCH_UNLOCK | SECRET_SEARCH_ALL,
                         NULL,
                         (GAsyncReadyCallback)store_form_data_cb,
                         single);
  g_hash_table_unref (attributes);
}

static void
free_form_auth_data_list (gpointer data)
{
  GSList *p, *l = (GSList*)data;

  for (p = l; p; p = p->next)
    form_auth_data_free ((EphyEmbedSingleFormAuthData*)p->data);

  g_slist_free (l);
}

static void
remove_form_auth_data (gpointer key, gpointer value, gpointer user_data)
{
  if (value)
    free_form_auth_data_list ((GSList*)value);
}

static void
ephy_embed_single_dispose (GObject *object)
{
  EphyEmbedSinglePrivate *priv = EPHY_EMBED_SINGLE (object)->priv;

#ifndef HAVE_WEBKIT2
  if (priv->cache) {
    soup_cache_flush (priv->cache);
    soup_cache_dump (priv->cache);
    g_object_unref (priv->cache);
    priv->cache = NULL;
  }
#endif

  G_OBJECT_CLASS (ephy_embed_single_parent_class)->dispose (object);
}

static void
ephy_embed_single_finalize (GObject *object)
{
  EphyEmbedSinglePrivate *priv = EPHY_EMBED_SINGLE (object)->priv;

  if (priv->form_auth_data) {
    g_hash_table_foreach (priv->form_auth_data,
                          (GHFunc)remove_form_auth_data,
                          NULL);
    g_hash_table_destroy (priv->form_auth_data);
  }

  G_OBJECT_CLASS (ephy_embed_single_parent_class)->finalize (object);
}

static void
ephy_embed_single_init (EphyEmbedSingle *single)
{
  EphyEmbedSinglePrivate *priv;

  single->priv = priv = EPHY_EMBED_SINGLE_GET_PRIVATE (single);

  priv->form_auth_data = g_hash_table_new_full (g_str_hash,
                                                g_str_equal,
                                                g_free,
                                                NULL);
  cache_secret_form_data (single);
}

static void
ephy_embed_single_class_init (EphyEmbedSingleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ephy_embed_single_finalize;
  object_class->dispose = ephy_embed_single_dispose;

  g_type_class_add_private (object_class, sizeof (EphyEmbedSinglePrivate));
}

#ifndef HAVE_WEBKIT2
static void
cache_size_cb (GSettings *settings,
               char *key,
               EphyEmbedSingle *single)
{
  int new_cache_size = g_settings_get_int (settings, key);
  soup_cache_set_max_size (single->priv->cache, new_cache_size * 1024 * 1024 /* in bytes */);
}
#endif

#ifdef HAVE_WEBKIT2
static void
complete_about_request_for_contents (WebKitURISchemeRequest *request,
                                     gchar *data,
                                     gsize data_length)
{
  GInputStream *stream;

  stream = g_memory_input_stream_new_from_data (data, data_length, g_free);
  webkit_uri_scheme_request_finish (request, stream, data_length, "text/html");
  g_object_unref (stream);
}

static void
get_plugins_cb (WebKitWebContext *web_context,
                GAsyncResult *result,
                WebKitURISchemeRequest *request)
{
  GList *plugins;
  GString *data_str;
  gsize data_length;

  data_str = g_string_new("<html>");
  plugins = webkit_web_context_get_plugins_finish (web_context, result, NULL);
  _ephy_about_handler_handle_plugins (data_str, plugins);
  g_string_append (data_str, "</html>");

  data_length = data_str->len;
  complete_about_request_for_contents (request, g_string_free (data_str, FALSE), data_length);
  g_object_unref (request);
}

static void
about_request_cb (WebKitURISchemeRequest *request,
                  gpointer user_data)
{
  const gchar *path;

  path = webkit_uri_scheme_request_get_path (request);
  if (!g_strcmp0 (path, "plugins")) {
    /* Plugins API is async in WebKit2 */
    webkit_web_context_get_plugins (webkit_web_context_get_default (),
                                    NULL,
                                    (GAsyncReadyCallback) get_plugins_cb,
                                    g_object_ref (request));
  } else {
    GString *contents;
    gsize data_length;

    contents = ephy_about_handler_handle (path);
    data_length = contents->len;
    complete_about_request_for_contents (request, g_string_free (contents, FALSE), data_length);
  }
}
#endif

/**
 * ephy_embed_single_initialize:
 * @single: the #EphyEmbedSingle
 * 
 * Performs startup initialisations. Must be called before calling
 * any other methods.
 **/
gboolean
ephy_embed_single_initialize (EphyEmbedSingle *single)
{
#ifdef HAVE_WEBKIT2
  WebKitWebContext *web_context;
  WebKitCookieManager *cookie_manager;
  char *filename;
  char *cookie_policy;

  /* TODO: SoupCache, SSL */
  web_context = webkit_web_context_get_default ();

  /* Store cookies in moz-compatible SQLite format */
  cookie_manager = webkit_web_context_get_cookie_manager (web_context);
  filename = g_build_filename (ephy_dot_dir (), "cookies.sqlite", NULL);
  webkit_cookie_manager_set_persistent_storage (cookie_manager, filename,
                                                WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE);
  g_free (filename);

  cookie_policy = g_settings_get_string (EPHY_SETTINGS_WEB,
                                         EPHY_PREFS_WEB_COOKIES_POLICY);
  ephy_embed_prefs_set_cookie_accept_policy (cookie_manager, cookie_policy);
  g_free (cookie_policy);

  /* about: URIs handler */
  webkit_web_context_register_uri_scheme (web_context,
                                          EPHY_ABOUT_SCHEME,
                                          about_request_cb,
                                          NULL, NULL);
#else
  SoupSession *session;
  SoupCookieJar *jar;
  char *filename;
  char *cookie_policy;
  char *cache_dir;
  char *favicon_db_path;
  EphyEmbedSinglePrivate *priv = single->priv;
  EphyEmbedShellMode mode;

  /* Initialise nspluginwrapper's plugins if available */
  if (g_file_test (NSPLUGINWRAPPER_SETUP, G_FILE_TEST_EXISTS) != FALSE)
    g_spawn_command_line_sync (NSPLUGINWRAPPER_SETUP, NULL, NULL, NULL, NULL);

  session = webkit_get_default_session ();

  /* Check SSL certificates */
  g_object_set (session,
                SOUP_SESSION_SSL_USE_SYSTEM_CA_FILE, TRUE,
                SOUP_SESSION_SSL_STRICT, FALSE,
                NULL);

  /* Store cookies in moz-compatible SQLite format */
  filename = g_build_filename (ephy_dot_dir (), "cookies.sqlite", NULL);
  jar = soup_cookie_jar_db_new (filename, FALSE);
  g_free (filename);
  cookie_policy = g_settings_get_string (EPHY_SETTINGS_WEB,
                                         EPHY_PREFS_WEB_COOKIES_POLICY);
  ephy_embed_prefs_set_cookie_jar_policy (jar, cookie_policy);
  g_free (cookie_policy);

  soup_session_add_feature (session, SOUP_SESSION_FEATURE (jar));
  g_object_unref (jar);

  /* Use GNOME proxy settings through libproxy. */
  soup_session_add_feature_by_type (session, SOUP_TYPE_PROXY_RESOLVER_DEFAULT);

  mode = ephy_embed_shell_get_mode (ephy_embed_shell_get_default ());

  /* WebKitSoupCache */
  cache_dir = g_build_filename (EPHY_EMBED_SHELL_MODE_HAS_PRIVATE_PROFILE (mode) ?
                                ephy_dot_dir () : g_get_user_cache_dir (),
                                g_get_prgname (), NULL);
  priv->cache = soup_cache_new (cache_dir, SOUP_CACHE_SINGLE_USER);
  g_free (cache_dir);

  soup_session_add_feature (session, SOUP_SESSION_FEATURE (priv->cache));
  /* Cache size in Mb: 1024 * 1024 */
  soup_cache_set_max_size (priv->cache, g_settings_get_int (EPHY_SETTINGS_WEB, EPHY_PREFS_CACHE_SIZE) << 20);
  soup_cache_load (priv->cache);

  g_signal_connect (EPHY_SETTINGS_WEB,
                    "changed::" EPHY_PREFS_CACHE_SIZE,
                    G_CALLBACK (cache_size_cb),
                    single);

  /* about: URIs handler */
  soup_session_add_feature_by_type (session, EPHY_TYPE_REQUEST_ABOUT);

  /* Initialize the favicon cache. */
  favicon_db_path = g_build_filename (g_get_user_data_dir (), g_get_prgname (), NULL);
  webkit_favicon_database_set_path (webkit_get_favicon_database (), favicon_db_path);
  g_free (favicon_db_path);
#endif
  return TRUE;
}

/**
 * ephy_embed_single_clear_cache:
 * @single: the #EphyEmbedSingle
 * 
 * Clears the HTTP cache (temporarily saved web pages).
 **/
void
ephy_embed_single_clear_cache (EphyEmbedSingle *single)
{
#ifdef HAVE_WEBKIT2
  webkit_web_context_clear_cache (webkit_web_context_get_default ());
#else
  soup_cache_clear (single->priv->cache);
#endif
}

/**
 * ephy_embed_single_get_form_auth:
 * @single: an #EphyEmbedSingle
 * @uri: the URI of a web page
 * 
 * Gets a #GSList of all stored login/passwords, in
 * #EphyEmbedSingleFormAuthData format, for any form in @uri, or %NULL
 * if we have none.
 * 
 * The #EphyEmbedSingleFormAuthData structs and the #GSList are owned
 * by @single and should not be freed by the user.
 * 
 * Returns: (transfer none) (element-type EphyEmbedSingleFormAuthData): #GSList with the possible auto-fills for the forms
 * in @uri, or %NULL
 **/
GSList *
ephy_embed_single_get_form_auth (EphyEmbedSingle *single,
                                 const char *uri)
{
  EphyEmbedSinglePrivate *priv;

  g_return_val_if_fail (EPHY_IS_EMBED_SINGLE (single), NULL);
  g_return_val_if_fail (uri, NULL);

  priv = single->priv;

  return g_hash_table_lookup (priv->form_auth_data, uri);
}

/**
 * ephy_embed_single_add_form_auth:
 * @single: an #EphyEmbedSingle
 * @uri: URI of the page
 * @form_username: name of the username input field
 * @form_password: name of the password input field
 * @username: username
 * 
 * Adds a new entry to the local cache of form auth data stored in
 * @single.
 * 
 **/
void
ephy_embed_single_add_form_auth (EphyEmbedSingle *single,
                                 const char *uri,
                                 const char *form_username,
                                 const char *form_password,
                                 const char *username)
{
  EphyEmbedSingleFormAuthData *form_data;
  EphyEmbedSinglePrivate *priv;
  GSList *l;

  g_return_if_fail (EPHY_IS_EMBED_SINGLE (single));
  g_return_if_fail (uri);
  g_return_if_fail (form_username);
  g_return_if_fail (form_password);
  g_return_if_fail (username);

  priv = single->priv;

  LOG ("Appending: name field: %s / pass field: %s / username: %s / uri: %s", form_username, form_password, username, uri);

  form_data = form_auth_data_new (form_username, form_password, username);
  l = g_hash_table_lookup (priv->form_auth_data,
                           uri);
  l = g_slist_append (l, form_data);
  g_hash_table_replace (priv->form_auth_data,
                        g_strdup (uri),
                        l);
}
