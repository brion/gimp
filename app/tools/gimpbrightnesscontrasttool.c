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

#include <string.h>

#include <gegl.h>
#include <gtk/gtk.h>

#include "libgimpconfig/gimpconfig.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "tools-types.h"

#include "operations/gimpbrightnesscontrastconfig.h"

#include "core/gimpdrawable.h"
#include "core/gimperror.h"
#include "core/gimpimage.h"

#include "widgets/gimphelp-ids.h"
#include "widgets/gimppropwidgets.h"
#include "widgets/gimpspinscale.h"
#include "widgets/gimpwidgets-constructors.h"

#include "display/gimpdisplay.h"

#include "gimpbrightnesscontrasttool.h"
#include "gimpimagemapoptions.h"
#include "gimptoolcontrol.h"

#include "gimp-intl.h"


#define SLIDER_WIDTH 200


static gboolean   gimp_brightness_contrast_tool_initialize (GimpTool              *tool,
                                                            GimpDisplay           *display,
                                                            GError               **error);
static void   gimp_brightness_contrast_tool_button_press   (GimpTool              *tool,
                                                            const GimpCoords      *coords,
                                                            guint32                time,
                                                            GdkModifierType        state,
                                                            GimpButtonPressType    press_type,
                                                            GimpDisplay           *display);
static void   gimp_brightness_contrast_tool_button_release (GimpTool              *tool,
                                                            const GimpCoords      *coords,
                                                            guint32                time,
                                                            GdkModifierType        state,
                                                            GimpButtonReleaseType  release_type,
                                                            GimpDisplay           *display);
static void   gimp_brightness_contrast_tool_motion         (GimpTool              *tool,
                                                            const GimpCoords      *coords,
                                                            guint32                time,
                                                            GdkModifierType        state,
                                                            GimpDisplay           *display);

static GeglNode *
              gimp_brightness_contrast_tool_get_operation  (GimpImageMapTool      *image_map_tool,
                                                            GObject              **config,
                                                            gchar                **undo_desc);
static void   gimp_brightness_contrast_tool_dialog         (GimpImageMapTool      *image_map_tool);

static void   brightness_contrast_to_levels_callback       (GtkWidget                  *widget,
                                                            GimpImageMapTool           *image_map_tool);


G_DEFINE_TYPE (GimpBrightnessContrastTool, gimp_brightness_contrast_tool,
               GIMP_TYPE_IMAGE_MAP_TOOL)

#define parent_class gimp_brightness_contrast_tool_parent_class


void
gimp_brightness_contrast_tool_register (GimpToolRegisterCallback  callback,
                                        gpointer                  data)
{
  (* callback) (GIMP_TYPE_BRIGHTNESS_CONTRAST_TOOL,
                GIMP_TYPE_IMAGE_MAP_OPTIONS, NULL,
                0,
                "gimp-brightness-contrast-tool",
                _("Brightness-Contrast"),
                _("Brightness/Contrast Tool: Adjust brightness and contrast"),
                N_("B_rightness-Contrast..."), NULL,
                NULL, GIMP_HELP_TOOL_BRIGHTNESS_CONTRAST,
                GIMP_STOCK_TOOL_BRIGHTNESS_CONTRAST,
                data);
}

static void
gimp_brightness_contrast_tool_class_init (GimpBrightnessContrastToolClass *klass)
{
  GimpToolClass         *tool_class    = GIMP_TOOL_CLASS (klass);
  GimpImageMapToolClass *im_tool_class = GIMP_IMAGE_MAP_TOOL_CLASS (klass);

  tool_class->initialize             = gimp_brightness_contrast_tool_initialize;
  tool_class->button_press           = gimp_brightness_contrast_tool_button_press;
  tool_class->button_release         = gimp_brightness_contrast_tool_button_release;
  tool_class->motion                 = gimp_brightness_contrast_tool_motion;

  im_tool_class->dialog_desc         = _("Adjust Brightness and Contrast");
  im_tool_class->settings_name       = "brightness-contrast";
  im_tool_class->import_dialog_title = _("Import Brightness-Contrast settings");
  im_tool_class->export_dialog_title = _("Export Brightness-Contrast settings");

  im_tool_class->get_operation       = gimp_brightness_contrast_tool_get_operation;
  im_tool_class->dialog              = gimp_brightness_contrast_tool_dialog;
}

static void
gimp_brightness_contrast_tool_init (GimpBrightnessContrastTool *bc_tool)
{
}

static gboolean
gimp_brightness_contrast_tool_initialize (GimpTool     *tool,
                                          GimpDisplay  *display,
                                          GError      **error)
{
  GimpBrightnessContrastTool *bc_tool  = GIMP_BRIGHTNESS_CONTRAST_TOOL (tool);
  GimpImage                  *image    = gimp_display_get_image (display);
  GimpDrawable               *drawable = gimp_image_get_active_drawable (image);

  if (! GIMP_TOOL_CLASS (parent_class)->initialize (tool, display, error))
    {
      return FALSE;
    }

  if (gimp_drawable_get_component_type (drawable) == GIMP_COMPONENT_TYPE_U8)
    {
      gimp_prop_widget_set_factor (bc_tool->brightness_scale,
                                   127.0, 1.0, 8.0, 0);
      gimp_prop_widget_set_factor (bc_tool->contrast_scale,
                                   127.0, 1.0, 8.0, 0);
    }
  else
    {
      gimp_prop_widget_set_factor (bc_tool->brightness_scale,
                                   0.5, 0.01, 0.1, 3);
      gimp_prop_widget_set_factor (bc_tool->contrast_scale,
                                   0.5, 0.01, 0.1, 3);
    }

  return TRUE;
}

static GeglNode *
gimp_brightness_contrast_tool_get_operation (GimpImageMapTool  *im_tool,
                                             GObject          **config,
                                             gchar            **undo_desc)
{
  GimpBrightnessContrastTool *bc_tool = GIMP_BRIGHTNESS_CONTRAST_TOOL (im_tool);

  bc_tool->config = g_object_new (GIMP_TYPE_BRIGHTNESS_CONTRAST_CONFIG, NULL);

  *config = G_OBJECT (bc_tool->config);

  return gegl_node_new_child (NULL,
                              "operation", "gimp:brightness-contrast",
                              "config",    bc_tool->config,
                              NULL);
}

static void
gimp_brightness_contrast_tool_button_press (GimpTool            *tool,
                                            const GimpCoords    *coords,
                                            guint32              time,
                                            GdkModifierType      state,
                                            GimpButtonPressType  press_type,
                                            GimpDisplay         *display)
{
  GimpBrightnessContrastTool *bc_tool = GIMP_BRIGHTNESS_CONTRAST_TOOL (tool);

  bc_tool->x  = coords->x - bc_tool->config->contrast   * 127.0;
  bc_tool->y  = coords->y + bc_tool->config->brightness * 127.0;
  bc_tool->dx =   bc_tool->config->contrast   * 127.0;
  bc_tool->dy = - bc_tool->config->brightness * 127.0;

  gimp_tool_control_activate (tool->control);
  tool->display = display;
}

static void
gimp_brightness_contrast_tool_button_release (GimpTool              *tool,
                                              const GimpCoords      *coords,
                                              guint32                time,
                                              GdkModifierType        state,
                                              GimpButtonReleaseType  release_type,
                                              GimpDisplay           *display)
{
  GimpBrightnessContrastTool *bc_tool = GIMP_BRIGHTNESS_CONTRAST_TOOL (tool);
  GimpImageMapTool           *im_tool = GIMP_IMAGE_MAP_TOOL (tool);

  gimp_tool_control_halt (tool->control);

  if (bc_tool->dx == 0 && bc_tool->dy == 0)
    return;

  if (release_type == GIMP_BUTTON_RELEASE_CANCEL)
    gimp_config_reset (GIMP_CONFIG (bc_tool->config));

  gimp_image_map_tool_preview (im_tool);
}

static void
gimp_brightness_contrast_tool_motion (GimpTool         *tool,
                                      const GimpCoords *coords,
                                      guint32           time,
                                      GdkModifierType   state,
                                      GimpDisplay      *display)
{
  GimpBrightnessContrastTool *bc_tool = GIMP_BRIGHTNESS_CONTRAST_TOOL (tool);

  bc_tool->dx =   (coords->x - bc_tool->x);
  bc_tool->dy = - (coords->y - bc_tool->y);

  g_object_set (bc_tool->config,
                "brightness", CLAMP (bc_tool->dy, -127.0, 127.0) / 127.0,
                "contrast",   CLAMP (bc_tool->dx, -127.0, 127.0) / 127.0,
                NULL);
}


/********************************/
/*  Brightness Contrast dialog  */
/********************************/

static void
gimp_brightness_contrast_tool_dialog (GimpImageMapTool *image_map_tool)
{
  GimpBrightnessContrastTool *bc_tool = GIMP_BRIGHTNESS_CONTRAST_TOOL (image_map_tool);
  GtkWidget                  *main_vbox;
  GtkWidget                  *scale;
  GtkWidget                  *button;

  main_vbox = gimp_image_map_tool_dialog_get_vbox (image_map_tool);

  /*  Create the brightness scale widget  */
  scale = gimp_prop_spin_scale_new (image_map_tool->config, "brightness",
                                    _("_Brightness"), 0.01, 0.1, 3);
  gtk_box_pack_start (GTK_BOX (main_vbox), scale, FALSE, FALSE, 0);
  gtk_widget_show (scale);

  bc_tool->brightness_scale = scale;

  /*  Create the contrast scale widget  */
  scale = gimp_prop_spin_scale_new (image_map_tool->config, "contrast",
                                    _("_Contrast"), 0.01, 0.1, 3);
  gtk_box_pack_start (GTK_BOX (main_vbox), scale, FALSE, FALSE, 0);
  gtk_widget_show (scale);

  bc_tool->contrast_scale = scale;

  button = gimp_stock_button_new (GIMP_STOCK_TOOL_LEVELS,
                                  _("Edit these Settings as Levels"));
  gtk_box_pack_start (GTK_BOX (main_vbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  g_signal_connect (button, "clicked",
                    G_CALLBACK (brightness_contrast_to_levels_callback),
                    image_map_tool);
}

static void
brightness_contrast_to_levels_callback (GtkWidget        *widget,
                                        GimpImageMapTool *image_map_tool)
{
  GimpLevelsConfig *levels;

  levels = gimp_brightness_contrast_config_to_levels_config (GIMP_BRIGHTNESS_CONTRAST_CONFIG (image_map_tool->config));

  gimp_image_map_tool_edit_as (image_map_tool,
                               "gimp-levels-tool",
                               GIMP_CONFIG (levels));

  g_object_unref (levels);
}
