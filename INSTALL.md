# Installing azMapGTK

Three ways to install, from most to least convenient on Arch:

1. [Arch package](#arch-package) — `makepkg -si`, map data included
2. [Build and install from source](#build-from-source) — any distro
3. [Run from the build directory](#run-without-installing) — for development

## Requirements

| Component | Minimum |
|-----------|---------|
| GTK | 4.0 |
| OpenGL | 3.3 core profile |
| libepoxy | any |
| shapelib | any |
| libcurl | any |
| CMake | 3.16 |
| C compiler | C11 |

A GPU (or software renderer) supporting the OpenGL 3.3 core profile is
required — the map is rendered in a `GtkGLArea` with a stencil buffer.

### Arch / Manjaro / CachyOS

```bash
sudo pacman -S gtk4 libepoxy shapelib curl cmake base-devel
```

### Debian / Ubuntu

```bash
sudo apt install libgtk-4-dev libepoxy-dev libshp-dev libcurl4-openssl-dev \
                 cmake build-essential
```

Note that Debian's shapelib package does not ship a pkg-config file. The build
falls back to `find_library`/`find_path`, so no extra steps are needed.

### Fedora

```bash
sudo dnf install gtk4-devel libepoxy-devel shapelib-devel libcurl-devel \
                 cmake gcc
```

## Arch package

`packaging/arch/PKGBUILD` builds the checkout it lives in and downloads the
Natural Earth data itself, so the resulting package is self-contained:

```bash
cd packaging/arch
makepkg -si
```

To build from a tree other than the one containing the PKGBUILD:

```bash
AZMAP_SRCROOT=/path/to/azMapGTK makepkg -si
```

Installed files:

| Path | Contents |
|------|----------|
| `/usr/bin/azmap-gtk` | Binary |
| `/usr/share/azmap-gtk/shaders/` | `map.vert`, `map.frag` |
| `/usr/share/azmap-gtk/data/` | Natural Earth 110m shapefiles (read-only master copy) |
| `/usr/share/applications/azmap-gtk.desktop` | Desktop entry |
| `/usr/share/doc/azmap-gtk/` | README, install, user, and developer guides, changelog |
| `/usr/share/licenses/azmap-gtk/LICENSE` | GPL-2.0-or-later |

Uninstall with `sudo pacman -R azmap-gtk`. Files created under your home
directory on first run (see below) are left behind; remove
`~/.config/azmap.conf` and `~/.local/azmap/` by hand if you want them gone.

The Natural Earth archives are pinned by SHA-256 in the PKGBUILD. If upstream
re-cuts those files, `makepkg` fails verification until the checksums are
refreshed with `updpkgsums`.

## Build from source

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

The default install prefix is `~/.local` (set in `CMakeLists.txt`, applied only
when CMake would otherwise use its own default):

```bash
make install
```

For a system-wide install:

```bash
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make -j$(nproc)
sudo make install
```

Both prefixes install to `${prefix}/bin`, `${prefix}/share/azmap-gtk/`, and
`${prefix}/share/applications/` via GNUInstallDirs. Map data is installed only
if a `data/` directory exists in the source tree, and only `.shp`, `.shx`,
`.dbf`, and `.prj` files are copied.

Build types: the default is `RelWithDebInfo`. Use `-DCMAKE_BUILD_TYPE=Debug` to
drop `_FORTIFY_SOURCE` and keep full symbols.

## Run without installing

```bash
./build/azmap-gtk
```

Shaders and map data are located relative to the executable, so the build tree
works as-is provided `shaders/` and `data/` sit next to `build/`. Shaders are
copied into `build/shaders/` automatically by the `copy_shaders` target.

## First run

On first launch azMapGTK creates `~/.config/azmap.conf` (mode 0600) with
built-in defaults:

```
name=NOCALL
lat=0.0
lon=90.0
qrz_user=
qrz_pass=
data_dir=~/.local/azmap/data
```

If `data_dir` contains no coastline shapefile, it is populated automatically
from the copy shipped with the installation (`/usr/share/azmap-gtk/data` for a
packaged install). That makes a fresh install usable with no downloads and no
arguments — though you will want to edit `name`, `lat`, and `lon` to your own
QTH before the distance and azimuth readouts mean anything.

Existing config files are never modified by this step.

## Map data

A packaged install already has the data. Otherwise, fetch the
[Natural Earth 110m](https://www.naturalearthdata.com/downloads/110m-physical-vectors/)
shapefiles:

| Layer | Required | Purpose |
|-------|----------|---------|
| `ne_110m_coastline` | yes | Coastline outlines |
| `ne_110m_land` | no | Land polygon fill |
| `ne_110m_admin_0_boundary_lines_land` | no | Country borders |

```bash
mkdir -p ~/.local/azmap/data && cd ~/.local/azmap/data
base=https://naciscdn.org/naturalearth/110m
for f in physical/ne_110m_coastline physical/ne_110m_land \
         cultural/ne_110m_admin_0_boundary_lines_land; do
    name=$(basename "$f")
    curl -sSLo "$name.zip" "$base/$f.zip"
    unzip -oq "$name.zip" -d "$name" && rm "$name.zip"
done
```

Layers may sit directly in `data_dir` or one subdirectory deep — both layouts
are searched. Anything deeper is not found.

## Verifying the install

```bash
azmap-gtk --help          # usage without opening a window
azmap-gtk                 # opens on your QTH from the config
```

Warnings about missing shapefiles are printed to stderr, so launch from a
terminal the first time.

## Troubleshooting

**`shapelib not found`** — install it (see [Requirements](#requirements)). On
Arch, `pkg-config --modversion shapelib` should print a version.

**Black window, no map** — the GL context failed or shaders were not found.
Run from a terminal; shader compile errors and the shader search path are
reported on stderr. Confirm OpenGL 3.3 with `glxinfo | grep "OpenGL core"`.

**Map draws, but land fill is missing** — the `ne_110m_land` layer is optional
and absent. Only coastline outlines are drawn without it.

**Land fill shows large grey wedges** — the land layer resolved to a polyline
shapefile (country borders) rather than land polygons. Check that `data_dir`
does not contain a stray `*_land.shp` that is not a polygon layer.

**Warning about config permissions** — `chmod 600 ~/.config/azmap.conf`. The
file holds your QRZ password. Configs created by the app are already 0600.

See the [User Guide](USER_GUIDE.md) for runtime troubleshooting and the
[Developer Guide](DEV_GUIDE.md) for build internals.
