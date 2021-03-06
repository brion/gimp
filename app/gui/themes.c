/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <errno.h>
#include <stdarg.h>

#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"

#include "gui-types.h"

#include "config/gimpguiconfig.h"

#include "core/gimp.h"

#include "themes.h"

#include "gimp-intl.h"


/*  local function prototypes  */

static void   themes_apply_theme         (Gimp                   *gimp,
                                          const gchar            *theme_name);
static void   themes_directories_foreach (const GimpDatafileData *file_data,
                                          gpointer                user_data);
static void   themes_list_themes_foreach (gpointer                key,
                                          gpointer                value,
                                          gpointer                data);
static void   themes_theme_change_notify (GimpGuiConfig          *config,
                                          GParamSpec             *pspec,
                                          Gimp                   *gimp);


/*  private variables  */

static GHashTable       *themes_hash           = NULL;
static GtkStyleProvider *themes_style_provider = NULL;


/*  public functions  */

void
themes_init (Gimp *gimp)
{
  GimpGuiConfig *config;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  config = GIMP_GUI_CONFIG (gimp->config);

  themes_hash = g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       g_free,
                                       g_free);

  if (config->theme_path)
    {
      gchar *path;

      path = gimp_config_path_expand (config->theme_path, TRUE, NULL);

      gimp_datafiles_read_directories (path,
                                       G_FILE_TEST_IS_DIR,
                                       themes_directories_foreach,
                                       gimp);

      g_free (path);
    }


  g_object_set (gtk_settings_get_for_screen (gdk_screen_get_default ()),
                "gtk-application-prefer-dark-theme", TRUE,
                NULL);

  themes_style_provider = GTK_STYLE_PROVIDER (gtk_css_provider_new ());

  /*  Use GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1 so theme files
   *  override default styles provided by widgets themselves.
   */
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             themes_style_provider,
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);

  g_object_unref (themes_style_provider);

  g_signal_connect (config, "notify::theme",
                    G_CALLBACK (themes_theme_change_notify),
                    gimp);

  themes_theme_change_notify (config, NULL, gimp);
}

void
themes_exit (Gimp *gimp)
{
  g_return_if_fail (GIMP_IS_GIMP (gimp));

  if (themes_hash)
    {
      g_signal_handlers_disconnect_by_func (gimp->config,
                                            themes_theme_change_notify,
                                            gimp);

      g_hash_table_destroy (themes_hash);
      themes_hash = NULL;
    }

  if (themes_style_provider)
    {
      g_object_unref (themes_style_provider);
      themes_style_provider = NULL;
    }
}

gchar **
themes_list_themes (Gimp *gimp,
                    gint *n_themes)
{

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);
  g_return_val_if_fail (n_themes != NULL, NULL);

  *n_themes = g_hash_table_size (themes_hash);

  if (*n_themes > 0)
    {
      gchar **themes;
      gchar **index;

      themes = g_new0 (gchar *, *n_themes + 1);

      index = themes;

      g_hash_table_foreach (themes_hash, themes_list_themes_foreach, &index);

      return themes;
    }

  return NULL;
}

const gchar *
themes_get_theme_dir (Gimp        *gimp,
                      const gchar *theme_name)
{
  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);

  if (! theme_name)
    theme_name = "Default";

  return g_hash_table_lookup (themes_hash, theme_name);
}

gchar *
themes_get_theme_file (Gimp        *gimp,
                       const gchar *first_component,
                       ...)
{
  GimpGuiConfig *gui_config;
  gchar         *file;
  gchar         *component;
  gchar         *path;
  va_list        args;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);
  g_return_val_if_fail (first_component != NULL, NULL);

  file = g_strdup (first_component);

  va_start (args, first_component);

  while ((component = va_arg (args, gchar *)))
    {
      gchar *tmp;

      tmp = g_build_filename (file, component, NULL);
      g_free (file);
      file = tmp;
    }

  va_end (args);

  gui_config = GIMP_GUI_CONFIG (gimp->config);

  path = g_build_filename (themes_get_theme_dir (gimp, gui_config->theme),
                           file, NULL);

  if (! g_file_test (path, G_FILE_TEST_EXISTS))
    {
      g_free (path);

      path = g_build_filename (themes_get_theme_dir (gimp, NULL),
                               file, NULL);
    }

  g_free (file);

  return path;
}


/*  private functions  */

static void
themes_apply_theme (Gimp        *gimp,
                    const gchar *theme_name)
{
  const gchar *theme_dir;
  gchar       *css_theme;
  gchar       *css_user;
  gchar       *theme_css;
  FILE        *file;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  theme_dir = themes_get_theme_dir (gimp, theme_name);

  if (theme_dir)
    {
      css_theme = g_build_filename (theme_dir, "gimp.css", NULL);
    }
  else
    {
      /*  get the hardcoded default theme css  */
      css_theme = g_build_filename (gimp_data_directory (),
                                    "themes", "Default", "gimp.css",
                                    NULL);
    }

  css_user = gimp_personal_rc_file ("gimp.css");

  theme_css = gimp_personal_rc_file ("theme.css");

  if (gimp->be_verbose)
    g_print ("Writing '%s'\n",
             gimp_filename_to_utf8 (theme_css));

  file = g_fopen (theme_css, "w");

  if (! file)
    {
      gimp_message (gimp, NULL, GIMP_MESSAGE_ERROR,
                    _("Could not open '%s' for writing: %s"),
                    gimp_filename_to_utf8 (theme_css), g_strerror (errno));
      goto cleanup;
    }

  {
    gchar *esc_css_theme = g_strescape (css_theme, NULL);
    gchar *esc_css_user  = g_strescape (css_user, NULL);

    fprintf (file,
             "/* GIMP theme.css */\n"
             "\n"
             "/* This file is written on GIMP startup and on every theme change.\n"
             " * It is NOT supposed to be edited manually. Edit your personal\n"
             " * gimp.css file instead (%s).\n"
             " */\n"
             "\n"
             "@import url(\"%s\");\n"
             "@import url(\"%s\");\n"
             "\n"
             "/* end of theme.css */\n",
             css_user,
             esc_css_theme,
             esc_css_user);

    g_free (esc_css_theme);
    g_free (esc_css_user);
  }

  fclose (file);

 cleanup:
  g_free (css_theme);
  g_free (css_user);
  g_free (theme_css);
}

static void
themes_directories_foreach (const GimpDatafileData *file_data,
                            gpointer                user_data)
{
  Gimp *gimp = GIMP (user_data);

  if (gimp->be_verbose)
    g_print ("Adding theme '%s' (%s)\n",
             gimp_filename_to_utf8 (file_data->basename),
             gimp_filename_to_utf8 (file_data->filename));

  g_hash_table_insert (themes_hash,
                       g_strdup (file_data->basename),
                       g_strdup (file_data->filename));
}

static void
themes_list_themes_foreach (gpointer key,
                            gpointer value,
                            gpointer data)
{
  gchar ***index = data;

  **index = g_strdup ((gchar *) key);

  (*index)++;
}

static void
themes_theme_change_notify (GimpGuiConfig *config,
                            GParamSpec    *pspec,
                            Gimp          *gimp)
{
  gchar  *theme_css;
  GError *error = NULL;

  themes_apply_theme (gimp, config->theme);

  theme_css = gimp_personal_rc_file ("theme.css");

  if (gimp->be_verbose)
    g_print ("Parsing '%s'\n",
             gimp_filename_to_utf8 (theme_css));

  if (! gtk_css_provider_load_from_path (GTK_CSS_PROVIDER (themes_style_provider),
                                         theme_css, &error))
    {
      g_printerr ("%s: error parsing %s: %s\n", G_STRFUNC,
                  gimp_filename_to_utf8 (theme_css), error->message);
      g_clear_error (&error);
    }

  g_free (theme_css);

  gtk_style_context_reset_widgets (gdk_screen_get_default ());
}
