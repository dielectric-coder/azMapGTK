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
/* text.h — Built-in vector stroke font for on-screen text.
 *
 * Renders uppercase A–Z, lowercase a–z, digits 0–9, and common punctuation
 * as GL_LINES segments.  Each glyph is defined in a normalized 0–1 cell;
 * text_build() scales and positions them into pixel-space vertex buffers.
 * No external font files or libraries required. */

#ifndef TEXT_H
#define TEXT_H

/* Initialize the stroke font lookup table. Call once at startup. */
void text_init(void);

/* Build line-segment vertices for a string.
 * x, y: top-left position in pixels (y increases downward).
 * size: character height in pixels.
 * out_verts: output buffer of x,y pairs (2 floats per vertex, 2 vertices per segment).
 * max_verts: capacity of out_verts in vertices (not floats).
 * Returns number of vertices written (always even, for GL_LINES). */
int text_build(const char *str, float x, float y, float size,
               float *out_verts, int max_verts);

/* Compute the rendered width of a string in pixels. */
float text_width(const char *str, float size);

#endif
