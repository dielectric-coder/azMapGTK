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
/* grid.c — Grid geometry generation for map overlays.
 *
 * Three grid types:
 * - grid_build(): AZEQ mode — concentric range rings + radial azimuth lines,
 *   drawn directly in km-space (no projection needed).
 * - grid_build_geo(): ORTHO mode — geographic parallels + meridians, projected
 *   through projection_forward() with back-hemisphere clipping.
 * - grid_build_dist_circles(): great-circle distance rings from the source
 *   location, computed via the forward geodesic formula and projected. */

#include <math.h>
#include <stdlib.h>
#include "grid.h"
#include "projection.h"

#define RING_STEP_KM   5000.0   /* distance between concentric range rings */
#define AZIMUTH_STEP    30.0    /* degrees between radial lines */
#define CIRCLE_PTS        72    /* points per ring */

/* Geographic graticule settings */
#define GEO_LAT_STEP   30.0    /* degrees between parallels */
#define GEO_LON_STEP   30.0    /* degrees between meridians */
#define GEO_SAMPLE_STEP 5.0    /* degrees between sample points */

void grid_build(MapData *md)
{
    md->vertex_count = 0;
    md->num_segments = 0;

    double max_r = EARTH_MAX_PROJ_RADIUS;   /* ~20015 km */
    int num_rings = (int)(max_r / RING_STEP_KM);
    int num_radials = (int)(360.0 / AZIMUTH_STEP);
    int max_verts = num_rings * (CIRCLE_PTS + 1) + num_radials * 2;

    free(md->vertices);
    md->vertices = malloc(max_verts * 2 * sizeof(float));
    if (!md->vertices) return;

    /* Concentric range rings */
    for (int ri = 1; ri <= num_rings; ri++) {
        float r = (float)(ri * RING_STEP_KM);
        int start = md->vertex_count;
        for (int i = 0; i <= CIRCLE_PTS && md->vertex_count < max_verts; i++) {
            float a = 2.0f * (float)M_PI * i / CIRCLE_PTS;
            md->vertices[md->vertex_count * 2]     = r * cosf(a);
            md->vertices[md->vertex_count * 2 + 1] = r * sinf(a);
            md->vertex_count++;
        }
        if (md->num_segments < MAX_SEGMENTS) {
            md->segment_starts[md->num_segments] = start;
            md->segment_counts[md->num_segments] = md->vertex_count - start;
            md->num_segments++;
        }
    }

    /* Radial azimuth lines from center to edge */
    for (int i = 0; i < num_radials && md->vertex_count + 2 <= max_verts; i++) {
        float a = 2.0f * (float)M_PI * i / num_radials;
        int start = md->vertex_count;
        md->vertices[md->vertex_count * 2]     = 0.0f;
        md->vertices[md->vertex_count * 2 + 1] = 0.0f;
        md->vertex_count++;
        md->vertices[md->vertex_count * 2]     = (float)max_r * cosf(a);
        md->vertices[md->vertex_count * 2 + 1] = (float)max_r * sinf(a);
        md->vertex_count++;
        if (md->num_segments < MAX_SEGMENTS) {
            md->segment_starts[md->num_segments] = start;
            md->segment_counts[md->num_segments] = 2;
            md->num_segments++;
        }
    }
}

void grid_build_geo(MapData *md)
{
    md->vertex_count = 0;
    md->num_segments = 0;

    /* Parallels and meridians — always use ±60° range so parallels land on
     * nice latitudes (0, ±30, ±60).  Mercator visibility extends to ±85°
     * but the ±60° parallels are sufficient. */
    double lat_max = 60.0;
    int num_parallels = (int)(2.0 * lat_max / GEO_LAT_STEP) + 1;
    int pts_per_parallel = (int)(360.0 / GEO_SAMPLE_STEP) + 1; /* 73 */
    int num_meridians = (int)(360.0 / GEO_LON_STEP);      /* 12 */
    int pts_per_meridian = (int)(180.0 / GEO_SAMPLE_STEP) + 1; /* 37 */
    int max_verts = num_parallels * pts_per_parallel + num_meridians * pts_per_meridian;

    free(md->vertices);
    md->vertices = malloc(max_verts * 2 * sizeof(float));
    if (!md->vertices) return;

    /* Parallels */
    for (double lat = -60.0; lat <= 60.0 + 0.01; lat += GEO_LAT_STEP) {
        int seg_start = md->vertex_count;
        int in_seg = 0;
        for (double lon = -180.0; lon <= 180.0 + 0.01 && md->vertex_count < max_verts; lon += GEO_SAMPLE_STEP) {
            double x, y;
            int rc = projection_forward(lat, lon, &x, &y);
            if (rc < 0) {
                /* Back hemisphere — flush current segment */
                if (in_seg >= 2 && md->num_segments < MAX_SEGMENTS) {
                    md->segment_starts[md->num_segments] = seg_start;
                    md->segment_counts[md->num_segments] = in_seg;
                    md->num_segments++;
                }
                /* Discard isolated point */
                if (in_seg == 1) md->vertex_count--;
                in_seg = 0;
                seg_start = md->vertex_count;
                continue;
            }
            md->vertices[md->vertex_count * 2]     = (float)x;
            md->vertices[md->vertex_count * 2 + 1] = (float)y;
            md->vertex_count++;
            in_seg++;
        }
        if (in_seg >= 2 && md->num_segments < MAX_SEGMENTS) {
            md->segment_starts[md->num_segments] = seg_start;
            md->segment_counts[md->num_segments] = in_seg;
            md->num_segments++;
        }
        if (in_seg == 1) md->vertex_count--;
    }

    /* Meridians */
    for (double lon = -180.0; lon < 180.0 - 0.01; lon += GEO_LON_STEP) {
        int seg_start = md->vertex_count;
        int in_seg = 0;
        for (double lat = -90.0; lat <= 90.0 + 0.01 && md->vertex_count < max_verts; lat += GEO_SAMPLE_STEP) {
            double x, y;
            int rc = projection_forward(lat, lon, &x, &y);
            if (rc < 0) {
                if (in_seg >= 2 && md->num_segments < MAX_SEGMENTS) {
                    md->segment_starts[md->num_segments] = seg_start;
                    md->segment_counts[md->num_segments] = in_seg;
                    md->num_segments++;
                }
                if (in_seg == 1) md->vertex_count--;
                in_seg = 0;
                seg_start = md->vertex_count;
                continue;
            }
            md->vertices[md->vertex_count * 2]     = (float)x;
            md->vertices[md->vertex_count * 2 + 1] = (float)y;
            md->vertex_count++;
            in_seg++;
        }
        if (in_seg >= 2 && md->num_segments < MAX_SEGMENTS) {
            md->segment_starts[md->num_segments] = seg_start;
            md->segment_counts[md->num_segments] = in_seg;
            md->num_segments++;
        }
        if (in_seg == 1) md->vertex_count--;
    }
}

#define DIST_CIRCLE_PTS 90  /* points per distance circle */

/* Compute destination point given start lat/lon (radians), distance (km), and bearing (radians). */
static void geo_destination(double lat1, double lon1, double dist_km, double bearing,
                            double *lat2, double *lon2)
{
    double d = dist_km / EARTH_RADIUS_KM;
    double sd = sin(d), cd = cos(d);
    double slat = sin(lat1), clat = cos(lat1);
    *lat2 = asin(slat * cd + clat * sd * cos(bearing));
    *lon2 = lon1 + atan2(sin(bearing) * sd * clat, cd - slat * sin(*lat2));
}

void grid_build_dist_circles(MapData *md, double center_lat, double center_lon)
{
    md->vertex_count = 0;
    md->num_segments = 0;

    double max_dist = EARTH_MAX_PROJ_RADIUS; /* ~20015 km */
    int num_circles = (int)(max_dist / DIST_CIRCLE_STEP_KM);
    int max_verts = num_circles * (DIST_CIRCLE_PTS + 1);

    free(md->vertices);
    md->vertices = malloc(max_verts * 2 * sizeof(float));
    if (!md->vertices) return;

    double clat = center_lat * M_PI / 180.0;
    double clon = center_lon * M_PI / 180.0;

    /* Mercator: projection_forward() always returns 0, so we detect
     * antimeridian wrapping by checking for large x-jumps instead. */
    int is_merc = (projection_get_mode() == PROJ_MERCATOR);
    float merc_split = (float)(M_PI * EARTH_RADIUS_KM);  /* half map width */

    for (int ri = 1; ri <= num_circles; ri++) {
        double dist_km = ri * DIST_CIRCLE_STEP_KM;
        int seg_start = md->vertex_count;
        int in_seg = 0;
        float prev_x = 0.0f;

        for (int i = 0; i <= DIST_CIRCLE_PTS && md->vertex_count < max_verts; i++) {
            double bearing = 2.0 * M_PI * i / DIST_CIRCLE_PTS;
            double dlat, dlon;
            geo_destination(clat, clon, dist_km, bearing, &dlat, &dlon);

            double lat_deg = dlat * 180.0 / M_PI;
            double lon_deg = dlon * 180.0 / M_PI;
            double x, y;
            int rc = projection_forward(lat_deg, lon_deg, &x, &y);
            int split = (rc < 0);
            if (!split && is_merc && in_seg > 0) {
                float dx = (float)x - prev_x;
                if (dx > merc_split || dx < -merc_split)
                    split = 1;
            }
            if (split) {
                if (in_seg >= 2 && md->num_segments < MAX_SEGMENTS) {
                    md->segment_starts[md->num_segments] = seg_start;
                    md->segment_counts[md->num_segments] = in_seg;
                    md->num_segments++;
                }
                if (in_seg == 1) md->vertex_count--;
                in_seg = 0;
                seg_start = md->vertex_count;
                continue;
            }
            prev_x = (float)x;
            md->vertices[md->vertex_count * 2]     = (float)x;
            md->vertices[md->vertex_count * 2 + 1] = (float)y;
            md->vertex_count++;
            in_seg++;
        }
        if (in_seg >= 2 && md->num_segments < MAX_SEGMENTS) {
            md->segment_starts[md->num_segments] = seg_start;
            md->segment_counts[md->num_segments] = in_seg;
            md->num_segments++;
        }
        if (in_seg == 1) md->vertex_count--;
    }
}
