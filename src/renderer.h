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
/* renderer.h — OpenGL shader management, VAO/VBO upload, and draw calls.
 *
 * Map-only renderer for GtkGLArea. Owns all GPU resources for the map
 * rendering layers. No sidebar/button/popup rendering — those are
 * handled by native GTK4 widgets. */

#ifndef RENDERER_H
#define RENDERER_H

#include "map_data.h"
#include "overlay.h"
#include "beacon.h"

typedef struct {
    unsigned int program;
    int          mvp_loc;
    int          color_loc;
    int          stipple_loc;

    /* Map coastline geometry */
    unsigned int map_vao;
    unsigned int map_vbo;
    int          map_segment_starts[MAX_SEGMENTS];
    int          map_segment_counts[MAX_SEGMENTS];
    int          map_num_segments;

    /* Country borders */
    unsigned int border_vao;
    unsigned int border_vbo;
    int          border_segment_starts[MAX_SEGMENTS];
    int          border_segment_counts[MAX_SEGMENTS];
    int          border_num_segments;

    /* Land polygons (filled via stencil buffer) */
    unsigned int land_vao;
    unsigned int land_vbo;
    int          land_segment_starts[MAX_SEGMENTS];
    int          land_segment_counts[MAX_SEGMENTS];
    int          land_segment_clamped[MAX_SEGMENTS];
    int          land_num_segments;

    /* Target line (great circle path, center -> target) */
    unsigned int line_vao;
    unsigned int line_vbo;
    int          line_vertex_count;

    /* Center marker (filled circle) */
    unsigned int center_marker_vao;
    unsigned int center_marker_vbo;
    int          center_marker_vcount;

    /* Target marker (outline circle) */
    unsigned int target_marker_vao;
    unsigned int target_marker_vbo;
    int          target_marker_vcount;

    /* North pole triangle (filled) */
    unsigned int npole_vao;
    unsigned int npole_vbo;

    /* Earth boundary circle */
    unsigned int circle_vao;
    unsigned int circle_vbo;
    int          circle_vertex_count;

    /* Earth filled disc (day-side base) */
    unsigned int disc_vao;
    unsigned int disc_vbo;
    int          disc_vertex_count;

    /* Grid (graticule) */
    unsigned int grid_vao;
    unsigned int grid_vbo;
    int          grid_segment_starts[MAX_SEGMENTS];
    int          grid_segment_counts[MAX_SEGMENTS];
    int          grid_num_segments;

    /* Distance circles from center (km-space) */
    unsigned int dist_vao;
    unsigned int dist_vbo;
    int          dist_segment_starts[MAX_SEGMENTS];
    int          dist_segment_counts[MAX_SEGMENTS];
    int          dist_num_segments;

    /* Distance circle labels (pixel-space GL_LINES) */
    unsigned int dist_label_vao;
    unsigned int dist_label_vbo;
    int          dist_label_vertex_count;

    /* Night overlay (filled triangles with per-vertex alpha, km-space) */
    unsigned int night_vao;
    unsigned int night_vbo;
    int          night_vertex_count;

    /* Aurora heatmap overlay */
    unsigned int aurora_vao;
    unsigned int aurora_vbo;
    int          aurora_vertex_count;

    /* DRAP absorption heatmap overlay */
    unsigned int drap_vao;
    unsigned int drap_vbo;
    int          drap_vertex_count;

    /* MUF contour lines (per-segment color, km-space) */
    unsigned int muf_vao;
    unsigned int muf_vbo;
    int          muf_segment_starts[MUF_MAX_SEGMENTS];
    int          muf_segment_counts[MUF_MAX_SEGMENTS];
    float        muf_segment_colors[MUF_MAX_SEGMENTS][4];
    int          muf_num_segments;

    /* Sporadic E contour lines (per-segment color, km-space) */
    unsigned int spore_vao;
    unsigned int spore_vbo;
    int          spore_segment_starts[MUF_MAX_SEGMENTS];
    int          spore_segment_counts[MUF_MAX_SEGMENTS];
    float        spore_segment_colors[MUF_MAX_SEGMENTS][4];
    int          spore_num_segments;

    /* Labels (pixel-space, positioned at marker screen coords) */
    unsigned int label_vao;
    unsigned int label_vbo;
    int          label_vertex_count;
    int          label_split;

    /* Label backgrounds (pixel-space, semi-transparent quads) */
    unsigned int label_bg_vao;
    unsigned int label_bg_vbo;
    int          label_bg_split;
    int          label_bg_vertex_count;

    /* NCDXF Beacons */
    unsigned int beacon_vao;
    unsigned int beacon_vbo;
    int          beacon_count;                        /* total vertices */
    int          beacon_inactive_count;               /* silent beacon vertices */
    int          beacon_band_start[NCDXF_BAND_COUNT]; /* per-band vertex offset */
    int          beacon_band_count[NCDXF_BAND_COUNT]; /* per-band vertex count */
    int          beacon_dirty;
} Renderer;

int renderer_init(Renderer *r, const char *shader_dir);

void renderer_upload_map(Renderer *r, const MapData *md);
void renderer_upload_borders(Renderer *r, const MapData *md);
void renderer_upload_land(Renderer *r, const MapData *md);
void renderer_upload_target_line(Renderer *r, const float *verts, int vertex_count);
void renderer_upload_markers(Renderer *r, float cx, float cy, float tx, float ty, float size_km);
void renderer_upload_npole(Renderer *r, float px, float py, float size_km);
void renderer_upload_earth_circle(Renderer *r, double radius);
void renderer_upload_grid(Renderer *r, const MapData *md);
void renderer_upload_dist_circles(Renderer *r, const MapData *md);
void renderer_upload_dist_labels(Renderer *r, float *verts, int vertex_count);
void renderer_upload_night(Renderer *r, const float *vertices, int vertex_count);
void renderer_upload_aurora(Renderer *r, const AuroraMesh *m);
void renderer_upload_drap(Renderer *r, const AuroraMesh *m);
void renderer_upload_muf(Renderer *r, const MufData *m);
void renderer_upload_spore(Renderer *r, const MufData *m);
void renderer_upload_labels(Renderer *r, float *verts, int vertex_count, int split);
void renderer_upload_label_bgs(Renderer *r, float *verts, int vertex_count, int split);

/* Beacon rendering */
void renderer_upload_beacons(Renderer *r, const BeaconSystem *sys);
void renderer_draw_beacons(const Renderer *r, const float *mvp);

/* Draw the map in the GtkGLArea viewport. */
void renderer_draw(const Renderer *r, const float *mvp, int fb_w, int fb_h);

void renderer_destroy(Renderer *r);

#endif
