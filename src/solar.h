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
/* solar.h — Subsolar point and solar zenith angle calculations.
 *
 * Computes the geographic point directly under the Sun for a given UTC time
 * (simplified declination model) and the solar zenith angle at any lat/lon.
 * Used by the day/night overlay to determine per-vertex shading. */

#ifndef SOLAR_H
#define SOLAR_H

#include <time.h>

typedef struct {
    double lat_deg;   /* subsolar latitude (solar declination) */
    double lon_deg;   /* subsolar longitude */
} SubsolarPoint;

/* Compute the subsolar point for a given UTC time. */
SubsolarPoint solar_subsolar_point(time_t utc_time);

/* Solar zenith angle (degrees) at a given lat/lon.
 * Returns > 90 for nighttime. */
double solar_zenith_angle(double lat_deg, double lon_deg,
                          const SubsolarPoint *sun);

#endif
