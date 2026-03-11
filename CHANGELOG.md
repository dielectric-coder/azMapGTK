# Changelog

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
