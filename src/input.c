/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) 2026 Michel Lachaine
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <https://www.gnu.org/licenses/>.
 */
/* input.c — GTK4 event controllers for the GtkGLArea map widget.
 *
 * Handles scroll (zoom), drag (pan), and keyboard input.
 * Replaces GLFW callbacks from the original azMap. */

#include <math.h>
#include "input.h"
#include "projection.h"

#define EARTH_R 6371.0
#define DEG2RAD (M_PI / 180.0)

static InputState *g_input;

/* Scroll → zoom */
static gboolean on_scroll(GtkEventControllerScroll *ctrl,
                           double dx, double dy, gpointer data)
{
    (void)ctrl; (void)dx; (void)data;
    InputState *is = g_input;
    if (dy > 0)
        camera_zoom(is->cam, 1.1f);
    else if (dy < 0)
        camera_zoom(is->cam, 0.9f);
    gtk_widget_queue_draw(is->gl_area);
    return TRUE;
}

/* Drag begin */
static void on_drag_begin(GtkGestureDrag *gesture, double x, double y, gpointer data)
{
    (void)gesture; (void)data;
    InputState *is = g_input;
    is->dragging = 1;
    is->press_x = x;
    is->press_y = y;
    is->last_mouse_x = x;
    is->last_mouse_y = y;
}

/* Drag update — pan the map.
 * In ORTHO mode: rotate the globe (change projection center).
 * In AZEQ mode: camera pan only (no reprojection). */
static void on_drag_update(GtkGestureDrag *gesture, double off_x, double off_y, gpointer data)
{
    (void)gesture; (void)data;
    InputState *is = g_input;
    if (!is->dragging) return;

    double cur_x = is->press_x + off_x;
    double cur_y = is->press_y + off_y;
    double dx = cur_x - is->last_mouse_x;
    double dy = cur_y - is->last_mouse_y;
    is->last_mouse_x = cur_x;
    is->last_mouse_y = cur_y;

    /* Get widget size for coordinate conversion */
    int h = gtk_widget_get_height(is->gl_area);
    if (h <= 0) return;

    int scale = gtk_widget_get_scale_factor(is->gl_area);
    int fb_h = h * scale;

    /* Convert pixel delta to km delta */
    double km_per_px = (double)is->cam->zoom_km / (double)fb_h;
    double dx_km = -dx * scale * km_per_px;
    double dy_km =  dy * scale * km_per_px;

    if (projection_get_mode() == PROJ_AZEQ) {
        /* AZEQ: camera pan only — no globe rotation */
        camera_pan(is->cam, (float)dx_km, (float)dy_km);
    } else {
        /* ORTHO: rotate globe by changing projection center */
        double dlat = dy_km / (EARTH_R * DEG2RAD);
        double cos_lat = cos(is->center_lat * DEG2RAD);
        if (fabs(cos_lat) < 1e-10) cos_lat = 1e-10;
        double dlon = dx_km / (EARTH_R * DEG2RAD * cos_lat);

        is->center_lat += dlat;
        is->center_lon += dlon;

        /* Clamp latitude */
        if (is->center_lat > 90.0) is->center_lat = 90.0;
        if (is->center_lat < -90.0) is->center_lat = -90.0;
        /* Wrap longitude */
        while (is->center_lon > 180.0) is->center_lon -= 360.0;
        while (is->center_lon < -180.0) is->center_lon += 360.0;

        is->center_dirty = 1;
    }
    gtk_widget_queue_draw(is->gl_area);
}

/* Drag end */
static void on_drag_end(GtkGestureDrag *gesture, double off_x, double off_y, gpointer data)
{
    (void)gesture; (void)off_x; (void)off_y; (void)data;
    g_input->dragging = 0;
}

/* Key press */
static gboolean on_key_pressed(GtkEventControllerKey *ctrl,
                                guint keyval, guint keycode,
                                GdkModifierType state, gpointer data)
{
    (void)ctrl; (void)keycode; (void)state; (void)data;
    InputState *is = g_input;

    double pan_step = is->cam->zoom_km / (EARTH_R * 2.0 * M_PI) * 30.0;
    float pan_km = (float)(pan_step * EARTH_R * DEG2RAD);

    switch (keyval) {
    case GDK_KEY_Left:
        if (projection_get_mode() == PROJ_AZEQ) {
            camera_pan(is->cam, -pan_km, 0.0f);
        } else {
            is->center_lon -= pan_step;
            if (is->center_lon < -180.0) is->center_lon += 360.0;
            is->center_dirty = 1;
        }
        gtk_widget_queue_draw(is->gl_area);
        return TRUE;
    case GDK_KEY_Right:
        if (projection_get_mode() == PROJ_AZEQ) {
            camera_pan(is->cam, pan_km, 0.0f);
        } else {
            is->center_lon += pan_step;
            if (is->center_lon > 180.0) is->center_lon -= 360.0;
            is->center_dirty = 1;
        }
        gtk_widget_queue_draw(is->gl_area);
        return TRUE;
    case GDK_KEY_Up:
        if (projection_get_mode() == PROJ_AZEQ) {
            camera_pan(is->cam, 0.0f, pan_km);
        } else {
            is->center_lat += pan_step;
            if (is->center_lat > 90.0) is->center_lat = 90.0;
            is->center_dirty = 1;
        }
        gtk_widget_queue_draw(is->gl_area);
        return TRUE;
    case GDK_KEY_Down:
        if (projection_get_mode() == PROJ_AZEQ) {
            camera_pan(is->cam, 0.0f, -pan_km);
        } else {
            is->center_lat -= pan_step;
            if (is->center_lat < -90.0) is->center_lat = -90.0;
            is->center_dirty = 1;
        }
        gtk_widget_queue_draw(is->gl_area);
        return TRUE;
    case GDK_KEY_r:
    case GDK_KEY_R:
        is->center_lat = is->original_center_lat;
        is->center_lon = is->original_center_lon;
        camera_reset(is->cam);
        is->center_dirty = 1;
        gtk_widget_queue_draw(is->gl_area);
        return TRUE;
    case GDK_KEY_x:
    case GDK_KEY_X:
        if (is->swap_cb)
            is->swap_cb(is, is->swap_cb_data);
        return TRUE;
    case GDK_KEY_q:
    case GDK_KEY_Q:
    case GDK_KEY_Escape: {
        GtkRoot *root = gtk_widget_get_root(is->gl_area);
        if (GTK_IS_WINDOW(root))
            gtk_window_close(GTK_WINDOW(root));
        return TRUE;
    }
    default:
        break;
    }
    return FALSE;
}

void input_init(InputState *is, GtkWidget *gl_area, GtkWidget *window,
                Camera *cam, double center_lat, double center_lon)
{
    memset(is, 0, sizeof(*is));
    is->cam = cam;
    is->gl_area = gl_area;
    is->center_lat = center_lat;
    is->center_lon = center_lon;
    is->original_center_lat = center_lat;
    is->original_center_lon = center_lon;
    g_input = is;

    /* Scroll controller on GL area */
    GtkEventController *scroll = gtk_event_controller_scroll_new(
        GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    g_signal_connect(scroll, "scroll", G_CALLBACK(on_scroll), NULL);
    gtk_widget_add_controller(gl_area, scroll);

    /* Drag gesture on GL area */
    GtkGesture *drag = gtk_gesture_drag_new();
    g_signal_connect(drag, "drag-begin", G_CALLBACK(on_drag_begin), NULL);
    g_signal_connect(drag, "drag-update", G_CALLBACK(on_drag_update), NULL);
    g_signal_connect(drag, "drag-end", G_CALLBACK(on_drag_end), NULL);
    gtk_widget_add_controller(gl_area, GTK_EVENT_CONTROLLER(drag));

    /* Key controller on the window */
    GtkEventController *key = gtk_event_controller_key_new();
    g_signal_connect(key, "key-pressed", G_CALLBACK(on_key_pressed), NULL);
    gtk_widget_add_controller(window, key);
}
