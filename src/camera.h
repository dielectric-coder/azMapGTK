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
/* camera.h — Orthographic view state and MVP matrix.
 *
 * Manages the 2D view into projected km-space: zoom level (visible diameter
 * in km), pan offset, and aspect ratio.  Produces a 4x4 column-major
 * orthographic matrix for OpenGL. */

#ifndef CAMERA_H
#define CAMERA_H

#define ZOOM_MIN_KM     10.0f      /* Max zoom in: 10 km diameter */
#define ZOOM_MAX_KM     40030.0f   /* Min zoom: full Earth circumference */
#define ZOOM_DEFAULT_KM 40030.0f

typedef struct {
    float zoom_km;   /* Visible diameter in km */
    float pan_x;     /* Pan offset in km */
    float pan_y;     /* Pan offset in km */
    float aspect;    /* Window width / height */
} Camera;

void camera_init(Camera *cam);

/* Zoom by a multiplicative factor (>1 zooms out, <1 zooms in). */
void camera_zoom(Camera *cam, float factor);

/* Pan by dx, dy in km. */
void camera_pan(Camera *cam, float dx_km, float dy_km);

/* Reset to default view. */
void camera_reset(Camera *cam);

/* Compute a 4x4 orthographic MVP matrix (column-major, for OpenGL). */
void camera_get_mvp(const Camera *cam, float *mat4);

#endif
