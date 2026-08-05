# azMapGTK User Guide

## Getting Started

azMapGTK is an interactive azimuthal/orthographic/Mercator map projection tool for amateur radio operators. It displays great circle paths, distance and azimuth bearings, NCDXF/IARU beacon activity, and live HF propagation data on a globe rendered with OpenGL.

See [INSTALL.md](INSTALL.md) if you have not installed it yet.

### First Launch

The first time you run azMapGTK it creates `~/.config/azmap.conf` with defaults — callsign `NOCALL` at 0°N 90°E — and populates your map data directory from the shapefiles shipped with the installation. It then opens centered on that default location, so **edit the config with your own QTH before the readouts mean anything**:

```
name = MyQTH
lat = 40.4168
lon = -3.7038
```

Afterwards, any of these forms work:

```bash
# Full: center + target coordinates
azmap-gtk 40.4168 -3.7038 48.8566 2.3522 -c Madrid -t Paris

# Short: target only (center read from config file)
azmap-gtk 48.8566 2.3522 -t Paris

# No arguments: center from config, no target
azmap-gtk
```

The two-argument and no-argument forms need `lat` and `lon` in the config file. If the config is missing or has no coordinates, azMapGTK prints its usage instead of guessing.

### Command-Line Options

| Flag | Argument | Description |
|------|----------|-------------|
| `-c` | NAME | Label for the center (QTH) location |
| `-t` | NAME | Label for the target location |
| `-d` | DETAIL | Station detail string (pipe-delimited: `station\|freq\|country\|site\|lang\|target`) |
| `-s` | PATH | Shapefile directory or .shp file (repeatable; auto-discovers coastline/border/land in directories) |

Positional arguments are decimal degrees. Negative values represent South latitude and West longitude.

## Configuration

azMapGTK reads and writes `~/.config/azmap.conf`. The file uses `key = value` format with `#` comments. It is created with defaults on first run if absent; an existing file is never overwritten, only appended to with session state.

### User-Managed Keys

Set these yourself:

```
name = MyQTH
lat = 40.4168
lon = -3.7038
qrz_user = YOURCALL
qrz_pass = yourpassword
data_dir = ~/.local/azmap/data
```

| Key | Default | Description |
|-----|---------|-------------|
| `name` | `NOCALL` | Display name for your station |
| `lat` | `0.0` | QTH latitude (decimal degrees) |
| `lon` | `90.0` | QTH longitude (decimal degrees) |
| `qrz_user` | empty | QRZ.com username (enables callsign lookup) |
| `qrz_pass` | empty | QRZ.com password |
| `data_dir` | `~/.local/azmap/data` | Where shapefiles are looked up |

A leading `~/` in `data_dir` is expanded to your home directory. Shapefiles may sit directly in that directory or one subdirectory deep — both layouts work, deeper nesting is not searched. If the directory holds no coastline layer, it is populated from the copy shipped with the installation the next time you start the app.

Because the file holds your QRZ password it is created mode 0600. If you create it by hand, `chmod 600 ~/.config/azmap.conf` — otherwise azMapGTK warns on every launch.

Decimal values accept either `.` or `,` as the separator, so configs written under any locale parse correctly.

### Auto-Saved Session State

These keys are written automatically on exit and restored on next launch. You generally don't need to edit them:

| Key | Description |
|-----|-------------|
| `target_lat` / `target_lon` | Last target coordinates |
| `target_name` | Last target label |
| `view_zoom_km` | Zoom level |
| `view_pan_x` / `view_pan_y` | Camera pan offset (AZEQ mode) |
| `view_proj_mode` | `azeq`, `ortho`, or `mercator` |
| `view_center_lat` / `view_center_lon` | Projection center (after panning/rotating) |
| `window_w` / `window_h` | Window dimensions |

## Controls

### Mouse

| Action | AZEQ | ORTHO | MERC |
|--------|------|-------|------|
| Scroll up | Zoom in | Zoom in | Zoom in |
| Scroll down | Zoom out | Zoom out | Zoom out |
| Drag | Pan camera | Rotate globe | Pan camera |

### Keyboard

| Key | AZEQ | ORTHO | MERC |
|-----|------|-------|------|
| ← / → | Pan camera | Rotate globe | Change center longitude |
| ↑ / ↓ | Pan camera | Rotate globe | Pan camera |
| R | Reset view (center + zoom) | Reset view | Reset view |
| X | Swap source (QTH) ↔ target | Swap | Swap |
| Q / Esc | Quit | Quit | Quit |

### Sidebar Buttons

**MAP section:**

| Button | Action |
|--------|--------|
| PROJ | Cycle projection: AZEQ → ORTHO → MERC → AZEQ. The current mode is shown underneath |
| HOME | Recenter on QTH without changing zoom level |

**LAYERS section:**

| Button | Action |
|--------|--------|
| Aurora | Toggle NOAA OVATION aurora oval overlay |
| E's | Toggle Sporadic E (foEs) contour overlay |
| MUF | Toggle Maximum Usable Frequency contour overlay |
| DRAP | Toggle D-Region Absorption Prediction overlay |
| Beacons | Toggle NCDXF/IARU beacon display |

Each layer button fetches data from the internet when first activated. Layers are drawn on top of the map when enabled and cleared immediately when toggled off. Beacons are computed locally from the clock and need no network.

**SOURCE section:**

| Button | Action |
|--------|--------|
| QRZ | Toggle QRZ.com callsign lookup popover |
| TARGET | Toggle manual target entry popover (Lat / Lon fields) |

For QRZ, type a callsign in the popover entry and press Enter. If found, the map recenters on the station and displays its info in the sidebar. The QRZ button highlights when the current target originated from a QRZ lookup, and clears when another source updates the station info. Requires `qrz_user` and `qrz_pass` in the config file.

TARGET sets a target from coordinates directly, without a lookup.

**DATA button** (above the legends) opens a popover listing each data feed with the age of its last successful fetch — useful for telling "quiet band" apart from "stale download".

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
- **SW** — Solar wind speed (km/s)
- **CH HSS** — Coronal Hole High Speed Stream status: "CH HSS" = currently active, "CH HSS exp" = expected in forecast period (parsed from SWPC discussion text)

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

### Mercator (MERC)
The familiar rectangular world map. Longitude wrapping is handled per coastline ring, so shapes crossing the antimeridian are drawn correctly instead of being smeared across the map. Dragging pans; the left and right arrow keys shift the center longitude.

Cycle modes with the PROJ button. Switching modes reprojects every layer and resets zoom to fit the earth. The current mode is saved to config on exit.

## Beacons

The Beacons layer tracks the NCDXF/IARU international beacon network — 18 beacons transmitting in a coordinated 3-minute cycle. Each 10-second slot has 5 beacons transmitting simultaneously, one per band:

| Band | Frequency |
|------|-----------|
| 20m | 14.100 MHz |
| 17m | 18.110 MHz |
| 15m | 21.150 MHz |
| 12m | 24.930 MHz |
| 10m | 28.200 MHz |

Active beacons pulse on the map with a radius that grows through the slot, colored by band. The sidebar shows the current beacon callsign and the seconds remaining in its slot, with a color key for the five bands. Timing is derived from your system clock, so keep it NTP-synced — the whole network schedule depends on it.

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
| Solar wind speed | NOAA SWPC | `services.swpc.noaa.gov` |
| CH HSS prediction | NOAA SWPC (discussion) | `services.swpc.noaa.gov` |
| Callsign lookup | QRZ.com | `xmldata.qrz.com` |

Data refreshes automatically on a timer. An internet connection is required for propagation overlays and indices; the map itself works offline with local shapefiles.

## FIFO IPC

azMapGTK creates a named pipe at `$XDG_RUNTIME_DIR/azmap-target.fifo` (typically `/run/user/<uid>/azmap-target.fifo`). External applications can write target coordinates to this FIFO to update the map in real time:

```bash
echo "48.8566,2.3522,Paris" > "$XDG_RUNTIME_DIR/azmap-target.fifo"
```

Format: `<lat>,<lon>,<name>` or `<lat>,<lon>,<name>|<detail fields>` — latitude and longitude in decimal degrees, with comma-separated station name and optional pipe-delimited detail fields. The FIFO is removed on application exit.

## Map Data

azMapGTK uses [Natural Earth](https://www.naturalearthdata.com/) 110m shapefiles:

| Shapefile | Required | Description |
|-----------|----------|-------------|
| `ne_110m_coastline` | Yes | Coastline outlines |
| `ne_110m_land` | No | Land polygon fill |
| `ne_110m_admin_0_boundary_lines_land` | No | Country borders |

They are searched for in this order:

1. `data_dir` from the config file, and its immediate subdirectories
2. `data/` next to the executable (build tree)
3. `<prefix>/share/azmap-gtk/data/` (installed)
4. `<prefix>/share/azmap/data/` (shared with the original azMap)

A packaged install already ships the data and copies it into `data_dir` on first run. To install it by hand, see [INSTALL.md](INSTALL.md#map-data), or symlink from an existing azMap installation:

```bash
ln -s /path/to/azMap/data ~/.local/azmap/data
```

The `-s` flag can override shapefile paths at runtime. It accepts either a directory (auto-discovers coastline, border, and land shapefiles by name pattern) or a direct `.shp` file path (used as coastline). The flag is repeatable — multiple `-s` entries are merged, with defaults used for any layer not provided.

## Troubleshooting

**Black screen / no map**: Ensure OpenGL 3.3+ is available and the `shaders/` directory is next to the binary (or installed to the data directory).

**No propagation data**: Check your internet connection. Indices show `--` until the first successful fetch.

**QRZ lookup fails**: Verify `qrz_user` and `qrz_pass` in `~/.config/azmap.conf`. A QRZ XML subscription is required for the lookup API.

**No land fill**: The land shapefile (`ne_110m_land`) is optional but recommended. Without it, only coastline outlines are drawn.

**Large grey wedges instead of continents**: The land layer resolved to a polyline shapefile rather than land polygons — the stencil fill then treats open lines as polygon edges. Check `data_dir` for a stray `*_land.shp` that is not a polygon layer.

**Beacons out of step**: Beacon timing comes from your system clock. Sync it (`timedatectl status`) — a few seconds of drift shifts the whole 3-minute schedule.

**Warning about config permissions**: `chmod 600 ~/.config/azmap.conf`. The file holds your QRZ password.

**Config not saving**: Ensure `~/.config/` exists and is writable.

**Wrong QTH after first run**: The generated config defaults to `NOCALL` at 0°N 90°E. Edit `name`, `lat`, and `lon` in `~/.config/azmap.conf`.
