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
