/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * gimpcellrenderertoggle.c
 * Copyright (C) 2003-2004  Sven Neumann <sven@gimp.org>
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <gtk/gtk.h>

#include "gimpwidgetstypes.h"

#include "gimpwidgetsmarshal.h"
#include "gimpcellrenderertoggle.h"


/**
 * SECTION: gimpcellrenderertoggle
 * @title: GimpCellRendererToggle
 * @short_description: A #GtkCellRendererToggle that displays icons instead
 *                     of a checkbox.
 *
 * A #GtkCellRendererToggle that displays icons instead of a checkbox.
 **/


#define DEFAULT_ICON_SIZE  GTK_ICON_SIZE_BUTTON


enum
{
  CLICKED,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_STOCK_ID,
  PROP_STOCK_SIZE
};


typedef struct _GimpCellRendererTogglePrivate GimpCellRendererTogglePrivate;

struct _GimpCellRendererTogglePrivate
{
  gchar       *stock_id;
  GtkIconSize  stock_size;
  GdkPixbuf   *pixbuf;
};

#define GET_PRIVATE(obj) G_TYPE_INSTANCE_GET_PRIVATE (obj, \
                                                      GIMP_TYPE_CELL_RENDERER_TOGGLE, \
                                                      GimpCellRendererTogglePrivate)


static void gimp_cell_renderer_toggle_finalize     (GObject              *object);
static void gimp_cell_renderer_toggle_get_property (GObject              *object,
                                                    guint                 param_id,
                                                    GValue               *value,
                                                    GParamSpec           *pspec);
static void gimp_cell_renderer_toggle_set_property (GObject              *object,
                                                    guint                 param_id,
                                                    const GValue         *value,
                                                    GParamSpec           *pspec);
static void gimp_cell_renderer_toggle_get_size     (GtkCellRenderer      *cell,
                                                    GtkWidget            *widget,
                                                    const GdkRectangle   *rectangle,
                                                    gint                 *x_offset,
                                                    gint                 *y_offset,
                                                    gint                 *width,
                                                    gint                 *height);
static void gimp_cell_renderer_toggle_render       (GtkCellRenderer      *cell,
                                                    cairo_t              *cr,
                                                    GtkWidget            *widget,
                                                    const GdkRectangle   *background_area,
                                                    const GdkRectangle   *cell_area,
                                                    GtkCellRendererState  flags);
static gboolean gimp_cell_renderer_toggle_activate (GtkCellRenderer      *cell,
                                                    GdkEvent             *event,
                                                    GtkWidget            *widget,
                                                    const gchar          *path,
                                                    const GdkRectangle   *background_area,
                                                    const GdkRectangle   *cell_area,
                                                    GtkCellRendererState  flags);
static void gimp_cell_renderer_toggle_create_pixbuf (GimpCellRendererToggle *toggle,
                                                     GtkWidget              *widget);


G_DEFINE_TYPE (GimpCellRendererToggle, gimp_cell_renderer_toggle,
               GTK_TYPE_CELL_RENDERER_TOGGLE)

#define parent_class gimp_cell_renderer_toggle_parent_class

static guint toggle_cell_signals[LAST_SIGNAL] = { 0 };


static void
gimp_cell_renderer_toggle_class_init (GimpCellRendererToggleClass *klass)
{
  GObjectClass         *object_class = G_OBJECT_CLASS (klass);
  GtkCellRendererClass *cell_class   = GTK_CELL_RENDERER_CLASS (klass);

  toggle_cell_signals[CLICKED] =
    g_signal_new ("clicked",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GimpCellRendererToggleClass, clicked),
                  NULL, NULL,
                  _gimp_widgets_marshal_VOID__STRING_FLAGS,
                  G_TYPE_NONE, 2,
                  G_TYPE_STRING,
                  GDK_TYPE_MODIFIER_TYPE);

  object_class->finalize     = gimp_cell_renderer_toggle_finalize;
  object_class->get_property = gimp_cell_renderer_toggle_get_property;
  object_class->set_property = gimp_cell_renderer_toggle_set_property;

  cell_class->get_size       = gimp_cell_renderer_toggle_get_size;
  cell_class->render         = gimp_cell_renderer_toggle_render;
  cell_class->activate       = gimp_cell_renderer_toggle_activate;

  g_object_class_install_property (object_class,
                                   PROP_STOCK_ID,
                                   g_param_spec_string ("stock-id",
                                                        NULL, NULL,
                                                        NULL,
                                                        GIMP_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class,
                                   PROP_STOCK_SIZE,
                                   g_param_spec_int ("stock-size",
                                                     NULL, NULL,
                                                     0, G_MAXINT,
                                                     DEFAULT_ICON_SIZE,
                                                     GIMP_PARAM_READWRITE |
                                                     G_PARAM_CONSTRUCT));

  g_type_class_add_private (object_class, sizeof (GimpCellRendererTogglePrivate));
}

static void
gimp_cell_renderer_toggle_init (GimpCellRendererToggle *toggle)
{
}

static void
gimp_cell_renderer_toggle_finalize (GObject *object)
{
  GimpCellRendererTogglePrivate *private = GET_PRIVATE (object);

  if (private->stock_id)
    {
      g_free (private->stock_id);
      private->stock_id = NULL;
    }

  if (private->pixbuf)
    {
      g_object_unref (private->pixbuf);
      private->pixbuf = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gimp_cell_renderer_toggle_get_property (GObject    *object,
                                        guint       param_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GimpCellRendererTogglePrivate *private = GET_PRIVATE (object);

  switch (param_id)
    {
    case PROP_STOCK_ID:
      g_value_set_string (value, private->stock_id);
      break;
    case PROP_STOCK_SIZE:
      g_value_set_int (value, private->stock_size);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gimp_cell_renderer_toggle_set_property (GObject      *object,
                                        guint         param_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GimpCellRendererTogglePrivate *private = GET_PRIVATE (object);

  switch (param_id)
    {
    case PROP_STOCK_ID:
      if (private->stock_id)
        g_free (private->stock_id);
      private->stock_id = g_value_dup_string (value);
      break;
    case PROP_STOCK_SIZE:
      private->stock_size = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }

  if (private->pixbuf)
    {
      g_object_unref (private->pixbuf);
      private->pixbuf = NULL;
    }
}

static void
gimp_cell_renderer_toggle_get_size (GtkCellRenderer    *cell,
                                    GtkWidget          *widget,
                                    const GdkRectangle *cell_area,
                                    gint               *x_offset,
                                    gint               *y_offset,
                                    gint               *width,
                                    gint               *height)
{
  GimpCellRendererToggle        *toggle  = GIMP_CELL_RENDERER_TOGGLE (cell);
  GimpCellRendererTogglePrivate *private = GET_PRIVATE (cell);
  GtkStyleContext               *context = gtk_widget_get_style_context (widget);
  GtkBorder                      border;
  gint                           calc_width;
  gint                           calc_height;
  gint                           pixbuf_width;
  gint                           pixbuf_height;
  gfloat                         xalign;
  gfloat                         yalign;
  gint                           xpad;
  gint                           ypad;

  if (! private->stock_id)
    {
      GTK_CELL_RENDERER_CLASS (parent_class)->get_size (cell,
                                                        widget,
                                                        cell_area,
                                                        x_offset, y_offset,
                                                        width, height);
      return;
    }

  gtk_style_context_save (context);

  gtk_cell_renderer_get_alignment (cell, &xalign, &yalign);
  gtk_cell_renderer_get_padding (cell, &xpad, &ypad);

  if (! private->pixbuf)
    gimp_cell_renderer_toggle_create_pixbuf (toggle, widget);

  pixbuf_width  = gdk_pixbuf_get_width  (private->pixbuf);
  pixbuf_height = gdk_pixbuf_get_height (private->pixbuf);

  calc_width  = pixbuf_width  + (gint) xpad * 2;
  calc_height = pixbuf_height + (gint) ypad * 2;

  gtk_style_context_add_class (context, GTK_STYLE_CLASS_BUTTON);

  gtk_style_context_get_border (context, 0, &border);
  calc_width  += border.left + border.right;
  calc_height += border.top + border.bottom;

  if (width)
    *width  = calc_width;

  if (height)
    *height = calc_height;

  if (cell_area)
    {
      if (x_offset)
        {
          *x_offset = (((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ?
                        (1.0 - xalign) : xalign) *
                       (cell_area->width - calc_width));
          *x_offset = MAX (*x_offset, 0);
        }

      if (y_offset)
        {
          *y_offset = yalign * (cell_area->height - calc_height);
          *y_offset = MAX (*y_offset, 0);
        }
    }

  gtk_style_context_restore (context);
}

static void
gimp_cell_renderer_toggle_render (GtkCellRenderer      *cell,
                                  cairo_t              *cr,
                                  GtkWidget            *widget,
                                  const GdkRectangle   *background_area,
                                  const GdkRectangle   *cell_area,
                                  GtkCellRendererState  flags)
{
  GimpCellRendererTogglePrivate *private = GET_PRIVATE (cell);
  GtkStyleContext               *context = gtk_widget_get_style_context (widget);
  GdkRectangle                   toggle_rect;
  GtkStateFlags                  state;
  gboolean                       active;
  gint                           xpad;
  gint                           ypad;

  if (! private->stock_id)
    {
      GTK_CELL_RENDERER_CLASS (parent_class)->render (cell, cr, widget,
                                                      background_area,
                                                      cell_area,
                                                      flags);
      return;
    }

  gimp_cell_renderer_toggle_get_size (cell, widget, cell_area,
                                      &toggle_rect.x,
                                      &toggle_rect.y,
                                      &toggle_rect.width,
                                      &toggle_rect.height);

  gtk_style_context_save (context);

  gtk_style_context_add_class (context, GTK_STYLE_CLASS_BUTTON);

  gtk_cell_renderer_get_padding (cell, &xpad, &ypad);

  toggle_rect.x      += cell_area->x + xpad;
  toggle_rect.y      += cell_area->y + ypad;
  toggle_rect.width  -= xpad * 2;
  toggle_rect.height -= ypad * 2;

  if (toggle_rect.width <= 0 || toggle_rect.height <= 0)
    return;

  state = gtk_cell_renderer_get_state (cell, widget, flags);

  active =
    gtk_cell_renderer_toggle_get_active (GTK_CELL_RENDERER_TOGGLE (cell));

  if (active)
    state |= GTK_STATE_FLAG_ACTIVE;

  if (! gtk_cell_renderer_toggle_get_activatable (GTK_CELL_RENDERER_TOGGLE (cell)))
    state |= GTK_STATE_FLAG_INSENSITIVE;

  gtk_style_context_set_state (context, state);

  if (state & GTK_STATE_FLAG_PRELIGHT)
    gtk_render_frame (context, cr,
                      toggle_rect.x,     toggle_rect.y,
                      toggle_rect.width, toggle_rect.height);

  if (active)
    {
      GtkBorder border;
      gboolean  inconsistent;

      gtk_style_context_get_border (context, 0, &border);
      toggle_rect.x      += border.left;
      toggle_rect.y      += border.top;
      toggle_rect.width  -= border.left + border.right;
      toggle_rect.height -= border.top + border.bottom;

      gdk_cairo_set_source_pixbuf (cr, private->pixbuf,
                                   toggle_rect.x, toggle_rect.y);
      cairo_paint (cr);

      g_object_get (cell,
                    "inconsistent", &inconsistent,
                    NULL);

      if (inconsistent)
        {
          GdkRGBA color;

          gtk_style_context_get_color (context, state, &color);
          gdk_cairo_set_source_rgba (cr, &color);
          cairo_set_line_width (cr, 1.5);
          cairo_move_to (cr,
                         toggle_rect.x + toggle_rect.width - 1,
                         toggle_rect.y + 1);
          cairo_line_to (cr,
                         toggle_rect.x + 1,
                         toggle_rect.y + toggle_rect.height - 1);
          cairo_stroke (cr);
        }
    }

  gtk_style_context_restore (context);
}

static gboolean
gimp_cell_renderer_toggle_activate (GtkCellRenderer      *cell,
                                    GdkEvent             *event,
                                    GtkWidget            *widget,
                                    const gchar          *path,
                                    const GdkRectangle   *background_area,
                                    const GdkRectangle   *cell_area,
                                    GtkCellRendererState  flags)
{
  GtkCellRendererToggle *toggle = GTK_CELL_RENDERER_TOGGLE (cell);

  if (gtk_cell_renderer_toggle_get_activatable (toggle))
    {
      GdkModifierType state = 0;

      if (event && ((GdkEventAny *) event)->type == GDK_BUTTON_PRESS)
        state = ((GdkEventButton *) event)->state;

      gimp_cell_renderer_toggle_clicked (GIMP_CELL_RENDERER_TOGGLE (cell),
                                         path, state);

      return TRUE;
    }

  return FALSE;
}

static void
gimp_cell_renderer_toggle_create_pixbuf (GimpCellRendererToggle *toggle,
                                         GtkWidget              *widget)
{
  GimpCellRendererTogglePrivate *private = GET_PRIVATE (toggle);

  if (private->pixbuf)
    g_object_unref (private->pixbuf);

  private->pixbuf = gtk_widget_render_icon_pixbuf (widget,
                                                   private->stock_id,
                                                   private->stock_size);
}


/**
 * gimp_cell_renderer_toggle_new:
 * @stock_id: the stock_id of the icon to use for the active state
 *
 * Creates a custom version of the #GtkCellRendererToggle. Instead of
 * showing the standard toggle button, it shows a stock icon if the
 * cell is active and no icon otherwise. This cell renderer is for
 * example used in the Layers treeview to indicate and control the
 * layer's visibility by showing %GIMP_STOCK_VISIBLE.
 *
 * Return value: a new #GimpCellRendererToggle
 *
 * Since: GIMP 2.2
 **/
GtkCellRenderer *
gimp_cell_renderer_toggle_new (const gchar *stock_id)
{
  return g_object_new (GIMP_TYPE_CELL_RENDERER_TOGGLE,
                       "stock_id", stock_id,
                       NULL);
}

/**
 * gimp_cell_renderer_toggle_clicked:
 * @cell:  a #GimpCellRendererToggle
 * @path:  the path to the clicked row
 * @state: the modifier state
 *
 * Emits the "clicked" signal from a #GimpCellRendererToggle.
 *
 * Since: GIMP 2.2
 **/
void
gimp_cell_renderer_toggle_clicked (GimpCellRendererToggle *cell,
                                   const gchar            *path,
                                   GdkModifierType         state)
{
  g_return_if_fail (GIMP_IS_CELL_RENDERER_TOGGLE (cell));
  g_return_if_fail (path != NULL);

  g_signal_emit (cell, toggle_cell_signals[CLICKED], 0, path, state);
}
