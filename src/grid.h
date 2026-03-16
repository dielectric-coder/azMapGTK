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
/* grid.h — Grid geometry generation (graticules and distance circles).
 *
 * Generates three types of grid overlays:
 * - AZEQ mode: concentric range rings (5000 km step) + radial azimuth lines
 * - ORTHO mode: geographic parallels + meridians through projection_forward()
 * - Distance circles: great-circle rings at 2000 km intervals from center */

#ifndef GRID_H
#define GRID_H

#include "map_data.h"

/* Build range-ring / azimuth-line grid for azimuthal equidistant mode.
 * Uses the current projection center. Caller must free with map_data_free(). */
void grid_build(MapData *md);

/* Build geographic graticule (parallels + meridians) for orthographic mode.
 * Uses projection_forward() so grid depends on current center. */
void grid_build_geo(MapData *md);

/* Build distance circles at fixed intervals from center_lat/lon.
 * Works in both projection modes. Caller must free with map_data_free(). */
void grid_build_dist_circles(MapData *md, double center_lat, double center_lon);

/* Number of distance circles and the interval in km. */
#define DIST_CIRCLE_STEP_KM  2000.0
#define DIST_CIRCLE_COUNT    10  /* 2000, 4000, ..., 20000 */

#endif
