# Changelog

## v0.2.0 — 2026-08-05

### Added

- NCDXF/IARU beacon layer — 18 beacons on the 3-minute network cycle, per-band color coding, pulsing radius animation for the active beacon, sidebar callsign/countdown and band legend
- Mercator projection mode, with per-ring longitude wrapping to avoid antimeridian artifacts; PROJ button now cycles AZEQ → ORTHO → MERC
- Default configuration written to `~/.config/azmap.conf` (mode 0600) on first run: `NOCALL` at 0°N 90°E, empty QRZ credentials, `data_dir=~/.local/azmap/data`
- `data_dir` is populated automatically on first run from the shapefiles shipped with the installation
- Tilde expansion for `data_dir`
- Shapefile discovery now searches one subdirectory deep, matching the per-layer folder layout Natural Earth archives unpack into
- Arch `PKGBUILD` under `packaging/arch/`, building the working tree and fetching the Natural Earth data with pinned checksums
- Desktop entry, installed to `${datadir}/applications`
- `INSTALL.md` and `DEV_GUIDE.md`

### Fixed

- Land layer could resolve to the country-border polylines: `*_land.shp` also matches `ne_110m_admin_0_boundary_lines_land.shp`, which sorts first, and stencil-filling those open lines drew large grey wedges over the map. The land lookup now rejects paths containing `boundary`
- A fresh install no longer printed usage and exited when launched with no arguments — there is now always a config with coordinates to fall back on

### Documentation

- User guide updated for Mercator, beacons, the TARGET and DATA buttons, the new config keys, and shapefile search order

## v0.1.4 — 2026-03-17

### Added

- Repeatable `-s` flag: accepts directories (auto-discovers coastline/border/land shapefiles by glob pattern) or direct `.shp` files; multiple sources are merged
- `map_data_load_append()` for merging additional shapefiles into an existing MapData
- No-argument startup mode: centers on QTH from config when no positional args given

### Fixed

- Memory leak on partial `realloc` failure in shapefile append (sequential allocation with early bail-out)
- Missing NUL termination safety on direct `.shp` path `strncpy`
- Added stderr warnings when glob-based shapefile discovery finds no matches in a directory

## v0.1.3 — 2026-03-16

### Added

- LICENSE file (GNU General Public License v2.0 or later)
- SPDX license identifiers and copyright headers in all source files, shaders, and CMakeLists.txt

## v0.1.2 — 2026-03-14

### Added

- Solar wind speed (SW km/s) in sidebar from SWPC solar-wind-speed summary
- Coronal Hole HSS status in sidebar — parsed from SWPC discussion text; shows "CH HSS" when active, "CH HSS exp" when forecast
- Swap source/target with `X` key — exchanges QTH and target, recenters projection on new QTH
- Station detail (`-d` flag) now displayed in sidebar on startup (was previously ignored)

### Fixed

- X-ray flare class always showing `--` — API returns a JSON array, parser now extracts first element
- FIFO station detail parsing refactored into shared helper (no behavior change)

## v0.1.1 — 2026-03-12

### Fixed

- Missing cleanup of FIFO event source, scales fetch, and X-ray fetch on shutdown (resource leaks)
- Mutex leak in fetch_start() when strdup() fails
- Potential NULL dereference in QRZ HTTP buffer allocation
- Shader glob pattern in CMakeLists.txt now matches only .vert/.frag files

### Updated

- Documentation: corrected FIFO IPC path to `$XDG_RUNTIME_DIR/azmap-target.fifo`
- README: added SFU/SSN, NOAA scales, and X-ray flare class to features list

## v0.1.0 — 2026-03-10

Initial GTK4 port of azMap.

### Added

- GTK4 application with GtkGLArea for OpenGL map rendering
- Native GTK4 sidebar with clocks, station info, distance/azimuth readouts
- Toggle buttons for overlay layers (Aurora, Sporadic E, MUF, DRAP) in vertical layout
- ORTHO/HOME control buttons
- QRZ callsign lookup via popover entry
- Projection mode toggle (azimuthal equidistant / orthographic) with saved state on startup
- Day/night terminator with wider twilight gradient in AZEQ mode (120 radial divisions)
- Separate QTH (home) coordinates from panned projection center
- AZEQ mode: drag/arrow keys pan camera only (no globe rotation)
- ORTHO mode: drag/arrow keys rotate the globe
- HOME button recenters view without changing zoom level
- Layer toggle clears overlay immediately when deactivated
- FIFO IPC for receiving target updates from external applications
- Window icon and desktop entry support
- Default window size ensures square earth display panel

### Changed from azMap

- Replaced GLFW3 with GTK4 (GtkApplication, GtkGLArea, event controllers)
- Replaced GLEW with libepoxy for GL loading
- Removed hand-drawn OpenGL sidebar, buttons, popup, and legend rendering
- Simplified renderer (map layers only, no UI geometry)
- GLib main loop replaces GLFW event loop (g_timeout_add, g_io_add_watch)
