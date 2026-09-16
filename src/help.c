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
/* help.c — shortcut reference window, toggled with 'h'.
 *
 * Styling comes from the application stylesheet in main.c (.help-window and
 * friends), so this window follows the dark map UI rather than the theme. */

#include "help.h"

/* Only ever one window; cleared by on_help_destroy when it goes away. */
static GtkWidget *help_win;

typedef struct {
    const char *keys;   /* NULL marks a section heading, using `desc` as title */
    const char *desc;
} HelpRow;

static const HelpRow HELP_ROWS[] = {
    { NULL,        "MOUSE" },
    { "Scroll",    "Zoom in / out" },
    { "Drag",      "Pan the map — rotates the globe in ORTHO" },

    { NULL,        "KEYBOARD" },
    { "\xe2\x86\x90 \xe2\x86\x92",  "Pan (AZEQ) \xc2\xb7 rotate (ORTHO) \xc2\xb7 center lon (MERC)" },
    { "\xe2\x86\x91 \xe2\x86\x93",  "Pan (AZEQ, MERC) \xc2\xb7 rotate (ORTHO)" },
    { "R",         "Reset view — center and zoom" },
    { "X",         "Swap source (QTH) \xe2\x86\x94 target" },
    { "H",         "Show or hide this window" },
    { "Q  Esc",    "Quit" },
};

static void on_help_destroy(GtkWidget *w, gpointer data)
{
    (void)w; (void)data;
    help_win = NULL;
}

/* Esc, q and h all dismiss the window. It holds the focus while open, so the
 * main window's key controller never sees these. */
static gboolean on_help_key(GtkEventControllerKey *ctrl, guint keyval,
                            guint keycode, GdkModifierType state, gpointer data)
{
    (void)ctrl; (void)keycode; (void)state;
    switch (keyval) {
    case GDK_KEY_Escape:
    case GDK_KEY_h:
    case GDK_KEY_H:
    case GDK_KEY_q:
    case GDK_KEY_Q:
        gtk_window_close(GTK_WINDOW(data));
        return TRUE;
    default:
        return FALSE;
    }
}

static GtkWidget *build_help_window(GtkWindow *parent)
{
    GtkWidget *win = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(win), "azMap \xe2\x80\x94 Shortcuts");
    gtk_window_set_transient_for(GTK_WINDOW(win), parent);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(win), TRUE);
    gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
    gtk_widget_add_css_class(win, "help-window");

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(box, 16);
    gtk_widget_set_margin_end(box, 16);
    gtk_widget_set_margin_top(box, 12);
    gtk_widget_set_margin_bottom(box, 12);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 18);

    int row = 0;
    for (size_t i = 0; i < G_N_ELEMENTS(HELP_ROWS); i++) {
        const HelpRow *r = &HELP_ROWS[i];
        if (!r->keys) {
            GtkWidget *sec = gtk_label_new(r->desc);
            gtk_widget_add_css_class(sec, "help-section");
            gtk_label_set_xalign(GTK_LABEL(sec), 0.0f);
            gtk_grid_attach(GTK_GRID(grid), sec, 0, row++, 2, 1);
            continue;
        }
        GtkWidget *key = gtk_label_new(r->keys);
        gtk_widget_add_css_class(key, "help-key");
        gtk_label_set_xalign(GTK_LABEL(key), 0.0f);
        gtk_grid_attach(GTK_GRID(grid), key, 0, row, 1, 1);

        GtkWidget *desc = gtk_label_new(r->desc);
        gtk_widget_add_css_class(desc, "help-desc");
        gtk_label_set_xalign(GTK_LABEL(desc), 0.0f);
        gtk_grid_attach(GTK_GRID(grid), desc, 1, row++, 1, 1);
    }
    gtk_box_append(GTK_BOX(box), grid);

    GtkWidget *hint = gtk_label_new("Esc, Q or H closes this window");
    gtk_widget_add_css_class(hint, "help-hint");
    gtk_widget_set_margin_top(hint, 12);
    gtk_box_append(GTK_BOX(box), hint);

    gtk_window_set_child(GTK_WINDOW(win), box);

    GtkEventController *key = gtk_event_controller_key_new();
    g_signal_connect(key, "key-pressed", G_CALLBACK(on_help_key), win);
    gtk_widget_add_controller(win, key);

    g_signal_connect(win, "destroy", G_CALLBACK(on_help_destroy), NULL);
    return win;
}

void help_toggle(GtkWindow *parent)
{
    if (help_win) {
        gtk_window_close(GTK_WINDOW(help_win));
        return;
    }
    help_win = build_help_window(parent);
    gtk_window_present(GTK_WINDOW(help_win));
}
