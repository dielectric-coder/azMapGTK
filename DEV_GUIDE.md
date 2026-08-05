# azMapGTK Developer Guide

A GTK4 port of azMap. The globe is rendered with OpenGL in a `GtkGLArea`; the
sidebar uses native GTK4 widgets rather than hand-drawn OpenGL UI.

See [INSTALL.md](INSTALL.md) for dependencies and build commands, and
[USER_GUIDE.md](USER_GUIDE.md) for runtime behavior.

## Layout

```
src/            core modules + GTK glue
shaders/        map.vert, map.frag
packaging/      azmap-gtk.desktop
packaging/arch/ PKGBUILD
data/           Natural Earth shapefiles (gitignored, not in the repo)
```

There are no tests and no linter. The project builds with `-Wall -Wextra`
plus `-fstack-protector-strong`, `-fstack-clash-protection`,
`-Werror=format-security`, and `-Wl,-z,relro,-z,now`.

## Module boundaries

Core modules are shared with the original azMap and **must not depend on GTK**:

| Module | Responsibility |
|--------|----------------|
| `projection.c` | Forward/inverse azimuthal equidistant, orthographic, Mercator (lat/lon ↔ km-space) |
| `map_data.c` | Loads `.shp` via shapelib into segmented vertex arrays; `map_data_load_append()` merges extra files |
| `grid.c` | Graticule lines and distance circles, emitted as `MapData` segments |
| `solar.c` | Subsolar point from UTC |
| `nightmesh.c` | Tessellated day/night mesh with per-vertex alpha for twilight |
| `overlay.c` | Parses fetched JSON/text into `MufData` and `AuroraMesh` |
| `beacon.c` | NCDXF/IARU beacon table, slot arithmetic, per-band colors |
| `fetch.c` | Async HTTP (libcurl multi + pthreads), callback on completion |
| `camera.c` | Zoom/pan state, builds the 4×4 orthographic MVP |
| `config.c` | `~/.config/azmap.conf` parsing, defaults, merge-write of session state |
| `qrz.c` | QRZ.com XML API callsign lookup |
| `text.c` | Vector stroke font — strings to `GL_LINES` arrays in pixel coords |

GTK-specific glue:

| Module | Responsibility |
|--------|----------------|
| `main.c` | `GtkApplication` lifecycle, sidebar widget tree, signals, timers, owns `AppState` |
| `input.c` | GTK4 gestures/keys → camera operations; uses `swap_cb` for actions needing `AppState` |
| `renderer.c` | All VAO/VBO resources, shader compilation, draw calls |

`renderer.c` uses epoxy rather than GLEW, matching GTK4's requirements.

## Rendering pipeline

One GLSL program (`shaders/map.vert` + `shaders/map.frag`) with three uniforms:

| Uniform | Type | Meaning |
|---------|------|---------|
| `u_mvp` | `mat4` | Model-view-projection |
| `u_color` | `vec4` | Flat color for the current draw |
| `u_stipple` | `float` | Dotted-line rendering when > 0 |

All geometry is 2D `(x, y)` floats. Two coordinate spaces are used per frame:

1. **km-space** — map geometry through the `camera.c` MVP. Drawn back to front:
   earth disc (stencil-filled land) → grid → distance circles → night overlay →
   aurora/DRAP → borders → coastlines → MUF/SporE contours → great circle →
   markers → north pole.
2. **pixel-space** — labels, drawn after the map layers with a plain
   orthographic matrix, using the `text.c` stroke font.

### Stencil land fill

Land is filled with the stencil buffer rather than triangulated:

1. The earth disc marks stencil bit 7.
2. Land polygons **invert** bits 0–6 (odd-even rule), so interiors end odd.
3. A fullscreen quad fills where stencil > `0x80`.

Mercator uses per-ring longitude wrapping instead of clipping, which avoids the
self-intersecting shortcut edges that otherwise appear at the antimeridian.

This is why the land layer must be a **polygon** shapefile. Feeding it a
polyline layer produces large wedge artifacts — see [Gotchas](#gotchas).

## Data flow

**Startup** — parse CLI args → `config_ensure_default()` → `config_load()` →
resolve shapefile paths → `map_data.c` loads → `projection.c` projects →
`renderer_upload_*()` pushes to the GPU.

**Interaction** — `input.c` registers GTK4 event controllers (scroll/drag/key)
on the `GtkGLArea`. Events update `camera.c`, trigger reprojection and
re-upload, then `gtk_gl_area_queue_render()`.

**Timers** — `g_timeout_add()` drives the night overlay (60 s), overlay fetches,
clock labels, and the beacon slot animation.

**FIFO IPC** — target coordinates are read from
`$XDG_RUNTIME_DIR/azmap-target.fifo`, for integration with external apps.

**Overlays** — `fetch.c` runs curl on a background thread; completion callbacks
are marshalled onto the main thread with `g_idle_add()`, then update overlay
state and re-upload. Kp/Bz, SFU/SSN, DRAP, solar wind, and CH HSS are fetched
unconditionally, not gated on the layer toggles.

## Key patterns

**Segmented geometry.** Everything drawable is a `MapData` with parallel
arrays: `vertices[]` (interleaved x,y), `segment_starts[]`, `segment_counts[]`.
Upload functions copy these into GPU buffers.

**Projection changes are full recalculations.** Switching modes requires
`map_data_project()` followed by `renderer_upload_*()` for *every* layer, resets
camera zoom to fit the earth, and rebuilds the night overlay immediately. The
three modes are `PROJ_AZEQ`, `PROJ_ORTHO`, `PROJ_MERCATOR`, cycled by the PROJ
button in that order.

**Shapefile discovery.** `find_shp_in_dir()` globs a directory and its immediate
subdirectories for a pattern, skipping paths that contain an optional `reject`
substring. `find_shp_in_dir_quiet()` is the same without the stderr warning —
use it for probes where a miss is expected.

**Config merge-write.** `config_save_state()` rewrites only the session-state
keys, preserving comments, ordering, and credentials. Adding a persisted key
means adding it to `state_keys[]` in `config.c` as well as the parser.

## Adding things

**A new overlay layer**

1. Add a fetch URL and a parser in `overlay.c` producing `MufData` or
   `AuroraMesh`.
2. Add a `renderer_upload_*()` and a draw call in `renderer.c`, positioned
   correctly in the back-to-front order.
3. Add a toggle button in the LAYERS section of `main.c` and wire its handler.
4. If it has a color scale, extend `rebuild_legends()`.

**A new projection mode**

1. Add the enum value and forward/inverse functions in `projection.c`.
2. Handle it in the PROJ cycle and mode label in `main.c`.
3. Check `input.c` — pan versus rotate behavior is per-mode.
4. Add the config string mapping in `config.c` for `view_proj_mode`.

**A new sidebar readout**

Add the `GtkLabel` in the sidebar tree in `main.c`, then update it from the
relevant fetch callback. Sections are separated by `.info-sep` (light grey) or
`.btn-sep` (white) styled separators; button groups use half-sidebar-width
centered containers.

## Sidebar structure

`SIDEBAR_WIDTH` is 260 px. Vertical `GtkBox` containing: clock → station info →
propagation indices → DATA button → legends → SOURCE (QRZ, TARGET) → LAYERS
(Aurora, E's, MUF, DRAP, Beacons) → MAP (PROJ + mode label, HOME).

Legend colors are applied through a `GtkCssProvider` rebuilt at runtime with
per-label classes (`lc0`, `lc1`, …), because GTK4's CSS parser rejects some
inline `rgba()` forms.

## Packaging

`packaging/arch/PKGBUILD` builds the working tree it lives in — `$startdir/../..`,
overridable with `AZMAP_SRCROOT`. The Natural Earth archives are `source=()`
entries with pinned SHA-256 sums, unpacked in `prepare()` into one directory per
layer to match the fallback layout the app expects.

After changing code, rebuild and reinstall with:

```bash
cd packaging/arch && makepkg -f && sudo pacman -U azmap-gtk-*.pkg.tar.zst
```

Bump `pkgver` in the PKGBUILD together with `project(... VERSION ...)` in
`CMakeLists.txt` for real releases.

## Gotchas

**`*_land.shp` is ambiguous.** It matches both `ne_110m_land.shp` and
`ne_110m_admin_0_boundary_lines_land.shp`, and `glob` returns results sorted, so
the border polylines sort first. The land lookup passes `"boundary"` as the
`reject` argument for exactly this reason. Loading polylines as the land layer
produces large grey wedges, because the stencil fill treats the open lines as
polygon edges.

**Subdirectory depth.** Shapefile discovery searches `dir/` and `dir/*/` only.
Deeper nesting is silently not found.

**`data_dir` is expanded, other paths are not.** `config.c` expands a leading
`~/` for `data_dir`. No other config value gets tilde expansion.

**Locale.** `config.c` and `overlay.c` parse decimals with a locale-tolerant
`strtod` wrapper accepting both `.` and `,`. Use it rather than bare `strtod`
for anything user- or network-supplied.

**Thread boundaries.** Fetch callbacks arrive on a worker thread. Never touch
GTK widgets or `AppState` there — hand off via `g_idle_add()`.

**No tests.** Verify changes by running the app. For path-resolution work, a
temporary `fprintf` of the resolved paths plus a run with a throwaway `HOME`
(`HOME=/tmp/x azmap-gtk`) exercises the first-run path without touching your
real config.
