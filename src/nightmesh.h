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
/* nightmesh.h — Day/night overlay mesh generation.
 *
 * Generates a polar triangle mesh (180 angular × 60 radial divisions)
 * covering the Earth disc.  Each vertex carries a per-vertex alpha derived
 * from the solar zenith angle (transparent in daylight, opaque at night,
 * smooth gradient through twilight).  Rebuilt every 60 seconds. */

#ifndef NIGHTMESH_H
#define NIGHTMESH_H

#include "solar.h"

typedef struct {
    float *vertices;      /* interleaved x, y, alpha (3 floats per vertex) */
    int    vertex_count;
    int    capacity;
} NightMesh;

void nightmesh_init(NightMesh *nm);
void nightmesh_build(NightMesh *nm, const SubsolarPoint *sun);
void nightmesh_free(NightMesh *nm);

#endif
