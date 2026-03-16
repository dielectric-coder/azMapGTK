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
/* input.h — GTK4 event controller setup for the GtkGLArea map widget.
 *
 * Installs scroll (zoom), drag (pan), and keyboard controllers.
 * Tracks the current projection center lat/lon and signals the app
 * via center_dirty when it changes. */

#ifndef INPUT_H
#define INPUT_H

#include <gtk/gtk.h>
#include "camera.h"

typedef struct InputState InputState;

/* Callback for swapping source ↔ target (triggered by 'x' key) */
typedef void (*InputSwapCallback)(InputState *is, void *user_data);

struct InputState {
    Camera *cam;
    GtkWidget *gl_area;
    int     dragging;
    double  press_x;
    double  press_y;
    double  last_mouse_x;
    double  last_mouse_y;
    double  center_lat, center_lon;
    double  original_center_lat, original_center_lon;
    int     center_dirty;
    InputSwapCallback swap_cb;
    void             *swap_cb_data;
};

/* Initialize input state and install GTK4 event controllers on the GL area. */
void input_init(InputState *is, GtkWidget *gl_area, GtkWidget *window,
                Camera *cam, double center_lat, double center_lon);

#endif
