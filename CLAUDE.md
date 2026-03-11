# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

azMapGTK is a GTK4 port of azMap — an interactive azimuthal/orthographic map projection application for amateur radio operators. The globe is rendered via OpenGL in a GtkGLArea widget; the sidebar uses native GTK4 widgets instead of hand-drawn OpenGL UI.

## Build

Dependencies: GTK4, libepoxy, shapelib, libcurl, OpenGL 3.3+

```bash
# Arch/Manjaro
sudo pacman -S gtk4 libepoxy shapelib curl cmake base-devel

# Debian/Ubuntu
sudo apt install libgtk-4-dev libepoxy-dev libshp-dev libcurl4-openssl-dev cmake build-essential

# Build
mkdir -p build && cd build && cmake .. && make

# Install (defaults to ~/.local via CMakeLists.txt)
make install
```

Install targets (via GNUInstallDirs):
- Binary: `${bindir}/azmap-gtk`
- Shaders: `${datadir}/azmap-gtk/shaders/`
- Map data: `${datadir}/azmap-gtk/data/` (optional, .shp/.shx/.dbf/.prj only)

There are no tests or linters configured. The project compiles with `-Wall -Wextra`.

## Run

```bash
./azmap-gtk <center_lat> <center_lon> <target_lat> <target_lon> [options]
./azmap-gtk <target_lat> <target_lon> [options]   # center from ~/.config/azmap.conf
```

Map data: Natural Earth 110m shapefiles in `data/` (or symlinked from `../azMap/data/`).

## Architecture

### Rendering Pipeline

The app uses a single GLSL shader program (`shaders/map.vert` + `shaders/map.frag`) with two uniforms: `u_mvp` (4x4 matrix) and `u_color` (vec4). All geometry is 2D (x,y floats). The rendering works in two coordinate spaces:

1. **km-space** — map geometry projected via `camera.c` MVP matrix. Layers drawn back-to-front: earth disc (stencil-filled land) → grid → distance circles → night overlay → aurora/DRAP → borders → coastlines → MUF/SporE contours → great circle line → markers → north pole.
2. **pixel-space** — labels rendered with a simple orthographic matrix after the map layers. Uses `text.c` vector stroke font (GL_LINES).

Land fill uses the **stencil buffer**: disc marks stencil bit 7, land polygons invert bits 0-6 (odd-even rule), then a fullscreen quad draws where stencil < 0x80.

### Data Flow

`main.c` holds all application state in a single `AppState` struct and orchestrates everything:
- **Startup**: parses CLI args → loads config → loads shapefiles via `map_data.c` → projects vertices via `projection.c` → uploads geometry to GPU via `renderer_upload_*()`.
- **Interaction**: `input.c` registers GTK4 event controllers (scroll/drag/key) on the GtkGLArea. Events update `camera.c` state, trigger reprojection, re-upload, and `gtk_gl_area_queue_render()`.
- **Timers**: GTK `g_timeout_add()` drives periodic updates — night overlay (60s), overlay data fetches, clock labels.
- **FIFO IPC**: reads target coords from `/tmp/azmap_fifo` for external app integration.

### Module Boundaries

Core modules (shared with original azMap, do not depend on GTK):
- `projection.c` — forward/inverse azimuthal equidistant + orthographic projections (lat/lon ↔ km-space)
- `map_data.c` — loads `.shp` files via shapelib, stores projected vertices in segmented arrays (`segment_starts[]`/`segment_counts[]`)
- `grid.c` — generates graticule lines + distance circles as MapData segments
- `solar.c` — subsolar point from UTC
- `nightmesh.c` — tessellated day/night overlay mesh with per-vertex alpha for twilight
- `overlay.c` — parses fetched JSON/text into MufData, AuroraMesh structs for MUF contours, Sporadic E, Aurora, DRAP
- `fetch.c` — async HTTP via libcurl multi + pthreads, callback-based completion
- `camera.c` — zoom/pan state, builds 4x4 orthographic MVP matrix
- `config.c` — parses `~/.config/azmap.conf` key=value pairs
- `qrz.c` — QRZ.com XML API callsign lookup (uses `cJSON.c` for some responses)
- `text.c` — vector stroke font: converts strings to GL_LINES vertex arrays in pixel coords

GTK4-specific (the "glue" layer):
- `main.c` — GtkApplication lifecycle, builds sidebar widget tree, wires signals/timers, owns AppState
- `input.c` — translates GTK4 gestures/keys into camera operations and state changes
- `renderer.c` — manages all VAO/VBO GPU resources, compiles shaders, executes draw calls

### Key Patterns

- All geometry uses the `MapData` struct with parallel arrays: `vertices[]` (interleaved x,y floats), `segment_starts[]`, `segment_counts[]`. Upload functions copy these into GPU buffers.
- Projection changes require full vertex recalculation: `map_data_project()` → `renderer_upload_*()` for every layer.
- Overlays (MUF, Aurora, etc.) are fetched asynchronously. `fetch.c` runs curl in a background thread; completion callbacks run on the main thread via `g_idle_add()` and update overlay state + re-upload.
- The renderer uses epoxy (not GLEW) for GL function loading, matching GTK4's requirements.
