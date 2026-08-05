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
/* config.h — Configuration file parser and state persistence (~/.config/azmap.conf).
 *
 * Loads user preferences (center location, QRZ credentials) and persisted
 * session state (target, view, window) from a simple key=value file.
 * Saves session state back using merge-write to preserve comments and
 * manual entries. */

#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    char name[128];
    double lat;
    double lon;
    int valid; /* 1 if both lat and lon were found */
    char qrz_user[64];
    char qrz_pass[64];

    /* Persisted target */
    double target_lat, target_lon;
    char   target_name[128];
    int    target_valid;       /* 1 if both target lat/lon found */

    /* Persisted view state */
    float  view_zoom_km;
    float  view_pan_x, view_pan_y;
    int    view_proj_mode;     /* 0=PROJ_AZEQ, 1=PROJ_ORTHO, 2=PROJ_MERCATOR */
    double view_center_lat, view_center_lon;
    int    view_valid;         /* 1 if view state keys found */

    /* Persisted window state */
    int    window_w, window_h; /* window size in screen coords (not framebuffer) */
    int    panel_visible;      /* sidebar panel open/closed */
    int    window_valid;       /* 1 if window_w and window_h found */

    /* Data directory override (shapefile search path) */
    char   data_dir[1024];    /* from config: data_dir = /path/to/shapefiles */
} Config;

/* Create ~/.config/azmap.conf with built-in defaults if it does not exist yet.
 * Returns 1 if a file was created, 0 if one was already there, -1 on error. */
int config_ensure_default(void);

/* Load config from ~/.config/azmap.conf. Returns 0 on success, -1 if not found/error. */
int config_load(Config *cfg);

/* Save session state (target + view) to ~/.config/azmap.conf using merge-write.
 * Preserves existing comments, ordering, and credentials. Returns 0 on success. */
int config_save_state(double target_lat, double target_lon, const char *target_name,
                      float zoom_km, float pan_x, float pan_y,
                      int proj_mode, double center_lat, double center_lon,
                      int window_w, int window_h, int panel_visible);

#endif
