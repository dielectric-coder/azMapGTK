# azMapGTK User Guide

## Getting Started

azMapGTK is an interactive azimuthal/orthographic map projection tool for amateur radio operators. It displays great circle paths, distance and azimuth bearings, and live HF propagation data on a globe rendered with OpenGL.

### First Launch

You need at minimum a center (your QTH) and a target station:

```bash
# Full: center + target coordinates
azmap-gtk 40.4168 -3.7038 48.8566 2.3522 -c Madrid -t Paris

# Short: target only (center read from config file)
azmap-gtk 48.8566 2.3522 -t Paris

# No arguments: restore last session from config
azmap-gtk
```

The no-argument form requires a config file with saved session state (created automatically after the first run).

### Command-Line Options

| Flag | Argument | Description |
|------|----------|-------------|
| `-c` | NAME | Label for the center (QTH) location |
| `-t` | NAME | Label for the target location |
| `-d` | DETAIL | Station detail string (pipe-delimited: `station\|freq\|country\|site\|lang\|target`) |
| `-s` | PATH | Override path to the coastline shapefile |

Positional arguments are decimal degrees. Negative values represent South latitude and West longitude.

## Configuration

azMapGTK reads and writes `~/.config/azmap.conf`. The file uses `key = value` format with `#` comments.

### User-Managed Keys

Set these yourself:

```
name = MyQTH
lat = 40.4168
lon = -3.7038
qrz_user = YOURCALL
qrz_pass = yourpassword
```

| Key | Description |
|-----|-------------|
| `name` | Display name for your station |
| `lat` | QTH latitude (decimal degrees) |
| `lon` | QTH longitude (decimal degrees) |
| `qrz_user` | QRZ.com username (enables callsign lookup) |
| `qrz_pass` | QRZ.com password |

### Auto-Saved Session State

These keys are written automatically on exit and restored on next launch. You generally don't need to edit them:

| Key | Description |
|-----|-------------|
| `target_lat` / `target_lon` | Last target coordinates |
| `target_name` | Last target label |
| `view_zoom_km` | Zoom level |
| `view_pan_x` / `view_pan_y` | Camera pan offset (AZEQ mode) |
| `view_proj_mode` | `azeq` or `ortho` |
| `view_center_lat` / `view_center_lon` | Projection center (after panning/rotating) |
| `window_w` / `window_h` | Window dimensions |

## Controls

### Mouse

| Action | AZEQ Mode | ORTHO Mode |
|--------|-----------|------------|
| Scroll up | Zoom in | Zoom in |
| Scroll down | Zoom out | Zoom out |
| Drag | Pan camera | Rotate globe |

### Keyboard

| Key | AZEQ Mode | ORTHO Mode |
|-----|-----------|------------|
| Arrow keys | Pan camera | Rotate globe |
| R | Reset view (center + zoom) | Reset view (center + zoom) |
| X | Swap source (QTH) ↔ target | Swap source (QTH) ↔ target |
| Q / Esc | Quit | Quit |

### Sidebar Buttons

**MAP section:**

| Button | Action |
|--------|--------|
| ORTHO / AZEQ | Toggle between orthographic and azimuthal equidistant projection |
| HOME | Recenter on QTH without changing zoom level |

**LAYERS section:**

| Button | Action |
|--------|--------|
| Aurora | Toggle NOAA OVATION aurora oval overlay |
| E's | Toggle Sporadic E (foEs) contour overlay |
| MUF | Toggle Maximum Usable Frequency contour overlay |
| DRAP | Toggle D-Region Absorption Prediction overlay |

Each layer button fetches data from the internet when first activated. Layers are drawn on top of the map when enabled and cleared immediately when toggled off.

**SOURCE section:**

| Button | Action |
|--------|--------|
| QRZ | Toggle QRZ.com callsign lookup popover |

Type a callsign in the popover entry and press Enter. If found, the map recenters on the station and displays its info in the sidebar. The QRZ button highlights when the current target originated from a QRZ lookup. Requires `qrz_user` and `qrz_pass` in the config file.

## Sidebar Display

The sidebar (left panel, 260px wide) shows:

### Clock
- **UTC** — Current UTC time
- **LOC** — Current local time

### Station Info
Visible when a target is set:
- **DIST** — Great circle distance in km
- **AZ TO** — Azimuth from center to target (degrees)
- **AZ FROM** — Azimuth from target to center (degrees)

### Propagation Indices
Always visible, fetched automatically on startup regardless of overlay toggles:
- **Kp** — Planetary K-index (geomagnetic activity, 0-9)
- **Bz** — IMF Bz component (nT; negative = geomagnetically active)
- **SFU** — Solar Flux Units (10.7 cm radio flux)
- **SSN** — Sunspot Number
- **X-ray flare class** — Current GOES X-ray flare classification (e.g., C1.2, M5.0, X1.0)
- **DRAP peak** — D-region absorption peak frequency (MHz)
- **R/S/G** — NOAA Space Weather Scales (Radio blackout / Solar radiation storm / Geomagnetic storm, 0-5)

### Legends
Color-coded legends appear dynamically when overlay data is loaded:
- **E's legend** — foEs values in MHz with color scale
- **MUF legend** — MUF values in MHz with color scale
- **DRAP legend** — Absorption levels with color scale

## Map Layers

Drawn back-to-front:

1. **Earth disc** — Blue ocean with stencil-filled land (grey)
2. **Graticule** — Latitude/longitude grid lines
3. **Distance circles** — Concentric rings at 2000 km intervals from your QTH
4. **Night overlay** — Day/night terminator with twilight gradient (updates every 60 seconds)
5. **Aurora / DRAP** — Mesh overlays (when enabled)
6. **Borders** — Country boundary lines (if border shapefile present)
7. **Coastlines** — Coastline outlines
8. **MUF / E's contours** — Propagation contour lines (E's rendered as dotted lines)
9. **Great circle line** — Path from center to target
10. **Markers** — Center (QTH) and target station markers with labels
11. **North pole** — Pole indicator

## Projection Modes

### Azimuthal Equidistant (AZEQ)
The default mode. All points are at their true distance and direction from the center. Great circle paths appear as straight lines from center. Distances are preserved radially. The camera can be panned without changing the projection center.

### Orthographic (ORTHO)
A perspective view of the globe as seen from space. Dragging rotates the globe (changes the projection center). Useful for visualizing the earth's curvature and relative positions of continents.

Toggle between modes with the ORTHO/AZEQ button. The current mode is saved to config on exit.

## Data Sources

Propagation and space weather data is fetched from:

| Data | Source | URL |
|------|--------|-----|
| MUF contours | KC2G | `prop.kc2g.com` |
| Sporadic E (foEs) | KC2G | `prop.kc2g.com` |
| Aurora oval | NOAA SWPC (OVATION) | `services.swpc.noaa.gov` |
| DRAP absorption | NOAA SWPC | `services.swpc.noaa.gov` |
| Kp index | NOAA SWPC | `services.swpc.noaa.gov` |
| Bz (IMF) | NOAA SWPC | `services.swpc.noaa.gov` |
| Solar indices (SFU/SSN) | NOAA SWPC | `services.swpc.noaa.gov` |
| NOAA scales (R/S/G) | NOAA SWPC | `services.swpc.noaa.gov` |
| X-ray flares | NOAA SWPC (GOES) | `services.swpc.noaa.gov` |
| Callsign lookup | QRZ.com | `xmldata.qrz.com` |

Data refreshes automatically on a timer. An internet connection is required for propagation overlays and indices; the map itself works offline with local shapefiles.

## FIFO IPC

azMapGTK creates a named pipe at `$XDG_RUNTIME_DIR/azmap-target.fifo` (typically `/run/user/<uid>/azmap-target.fifo`). External applications can write target coordinates to this FIFO to update the map in real time:

```bash
echo "48.8566,2.3522,Paris" > "$XDG_RUNTIME_DIR/azmap-target.fifo"
```

Format: `<lat>,<lon>,<name>` or `<lat>,<lon>,<name>|<detail fields>` — latitude and longitude in decimal degrees, with comma-separated station name and optional pipe-delimited detail fields. The FIFO is removed on application exit.

## Map Data

azMapGTK uses [Natural Earth](https://www.naturalearthdata.com/) 110m shapefiles. Place them in the `data/` directory relative to the binary:

| Shapefile | Required | Description |
|-----------|----------|-------------|
| `ne_110m_coastline` | Yes | Coastline outlines |
| `ne_110m_land` | No | Land polygon fill |
| `ne_110m_admin_0_boundary_lines_land` | No | Country borders |

Or symlink from an existing azMap installation:

```bash
ln -s /path/to/azMap/data data
```

The `-s` flag can override the coastline shapefile path at runtime.

## Troubleshooting

**Black screen / no map**: Ensure OpenGL 3.3+ is available and the `shaders/` directory is next to the binary (or installed to the data directory).

**No propagation data**: Check your internet connection. Indices show `--` until the first successful fetch.

**QRZ lookup fails**: Verify `qrz_user` and `qrz_pass` in `~/.config/azmap.conf`. A QRZ XML subscription is required for the lookup API.

**No land fill**: The land shapefile (`ne_110m_land`) is optional but recommended. Without it, only coastline outlines are drawn.

**Config not saving**: Ensure `~/.config/` exists and is writable.
