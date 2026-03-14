# azMapGTK

GTK4 port of [azMap](https://github.com/mikelisfbay/azMap) — an interactive azimuthal/orthographic map projection application for amateur radio operators. The globe is rendered via OpenGL in a GtkGLArea widget; the sidebar uses native GTK4 widgets.

![azMapGTK screenshot](doc/screenshot.png)

## Features

- Azimuthal equidistant and orthographic projection modes
- Great circle path, distance, and azimuth display
- Day/night terminator overlay with twilight gradient
- HF propagation overlays: MUF contours (KC2G), Sporadic E (KC2G), Aurora (NOAA OVATION), DRAP absorption (NOAA SWPC)
- Geomagnetic indices (Kp, Bz), solar indices (SFU, SSN)
- NOAA space weather scales and X-ray flare classification
- QRZ.com callsign lookup
- Distance circles (2000 km intervals)
- FIFO IPC for target updates from external applications (`$XDG_RUNTIME_DIR/azmap-target.fifo`)
- Zoom, pan, and keyboard navigation

## Dependencies

GTK4, libepoxy, shapelib, libcurl, OpenGL 3.3+

### Arch/Manjaro

```bash
sudo pacman -S gtk4 libepoxy shapelib curl
```

### Debian/Ubuntu

```bash
sudo apt install libgtk-4-dev libepoxy-dev libshp-dev libcurl4-openssl-dev
```

## Build

```bash
mkdir -p build && cd build && cmake .. && make
```

### Install

```bash
# Install to ~/.local (default)
cmake --install .

# Or system-wide
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
sudo cmake --install .
```

## Map Data

Download [Natural Earth 110m](https://www.naturalearthdata.com/downloads/110m-physical-vectors/) shapefiles into `data/`:

- **Coastlines** (required): `ne_110m_coastline`
- **Land polygons** (optional): `ne_110m_land`
- **Country borders** (optional): `ne_110m_admin_0_boundary_lines_land`

Or symlink from an existing azMap installation: `ln -s ../azMap/data data`

## Usage

```bash
./azmap-gtk <center_lat> <center_lon> <target_lat> <target_lon> [options]
./azmap-gtk <target_lat> <target_lon> [options]   # center from config
```

### Example

```bash
./azmap-gtk 40.4168 -3.7038 48.8566 2.3522 -c Madrid -t Paris
```

### Options

| Flag | Description |
|------|-------------|
| `-c NAME` | Center location name |
| `-t NAME` | Target location name |
| `-d DETAIL` | Station detail string (pipe-delimited) |
| `-s PATH` | Shapefile path override |

### Config File

Optional. Place at `~/.config/azmap.conf`:

```
name = MyQTH
lat = 40.4168
lon = -3.7038
qrz_user = YOURCALL
qrz_pass = yourpassword
```

## Controls

| Input | AZEQ Mode | ORTHO Mode |
|-------|-----------|------------|
| Scroll | Zoom in/out | Zoom in/out |
| Drag | Pan (camera) | Rotate globe |
| Arrow keys | Pan (camera) | Rotate globe |
| R | Reset view | Reset view |
| X | Swap source (QTH) ↔ target | Swap source (QTH) ↔ target |
| HOME button | Recenter (keep zoom) | Recenter (keep zoom) |
| Q / Esc | Quit | Quit |

## Architecture

Core math and data modules are shared with azMap (unchanged):

```
src/
├── main.c          GTK4 application, GtkGLArea + sidebar, timers, FIFO IPC
├── input.c         GTK4 event controllers (scroll/drag/key)
├── renderer.c      Map-only OpenGL renderer (no sidebar GL code)
├── projection.c    Azimuthal equidistant + orthographic projections
├── map_data.c      Shapefile loading, vertex arrays, boundary clipping
├── grid.c          Graticule + distance circles
├── overlay.c       MUF, Sporadic E, Aurora, DRAP overlays
├── nightmesh.c     Day/night overlay mesh
├── solar.c         Subsolar point calculation
├── fetch.c         Async HTTP (libcurl + pthread)
├── camera.c        View state (zoom, pan), MVP matrix
├── config.c        Config file parser
├── qrz.c          QRZ.com callsign lookup
├── text.c          Vector stroke font for GL labels
└── cJSON.c         Vendored JSON parser (MIT)
```

## License

Same license as azMap.
