# CLAUDE.md

## Project Overview

azMapGTK is a GTK4 port of azMap — an interactive azimuthal/orthographic map projection application. The map is rendered via OpenGL in a GtkGLArea widget; the sidebar uses native GTK4 widgets instead of hand-drawn OpenGL UI.

## Build

Dependencies: GTK4, libepoxy, shapelib, libcurl, OpenGL 3.3+

```bash
# Install dependencies (Arch/Manjaro)
sudo pacman -S gtk4 libepoxy shapelib curl

# Build
mkdir -p build && cd build && cmake .. && make
```

## Run

Same CLI args as azMap:
```bash
./azmap-gtk <center_lat> <center_lon> <target_lat> <target_lon> [options]
./azmap-gtk <target_lat> <target_lon> [options]   # center from config
```

Map data: symlinked from ../azMap/data/ (Natural Earth 110m shapefiles).

## Architecture

Core math/data modules are shared with azMap (unchanged):
- `projection.c` — azimuthal equidistant + orthographic projections
- `map_data.c` — shapefile loading, vertex arrays, reprojection
- `grid.c` — graticule + distance circles
- `solar.c` / `nightmesh.c` — day/night overlay
- `overlay.c` — MUF, Sporadic E, Aurora, DRAP overlays
- `fetch.c` — async HTTP (libcurl + pthread)
- `camera.c` — orthographic view state
- `config.c` — config file parser
- `qrz.c` — QRZ.com callsign lookup
- `text.c` — vector stroke font (for GL labels)

GTK4-specific files:
- `main.c` — GtkApplication, GtkGLArea + sidebar widget tree, timers, FIFO IPC
- `input.c` — GTK4 event controllers (scroll/drag/key) on GtkGLArea
- `renderer.c` — Map-only OpenGL renderer (no sidebar/button GL code)
