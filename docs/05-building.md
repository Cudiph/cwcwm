# Building on Ubuntu / Pop!_OS

Tested on Ubuntu 26.04 LTS and Pop!_OS 24.04 LTS. May also work on Ubuntu 24.04 LTS (see [notes below](#ubuntu-2404)).

Several dependencies must be built from source as these distros ship older versions.
An experimental automated build script is available at [`scripts/build-deps.sh`](../scripts/build-deps.sh).

## System packages

```bash
sudo add-apt-repository universe

sudo apt update

sudo apt install meson ninja-build wayland-protocols libwayland-dev \
  libcairo2-dev libxkbcommon-dev libinput-dev libxxhash-dev \
  libluajit-5.1-dev libxcb1-dev xwayland libdrm-dev lua-lgi \
  libpango1.0-dev gir1.2-pango-1.0 libzip-dev librsvg2-dev \
  libseat-dev libudev-dev libgbm-dev libgles-dev libegl-dev \
  libvulkan-dev glslang-tools liblcms2-dev libxcb-dri3-dev \
  libxcb-present-dev libxcb-render-util0-dev libxcb-shm0-dev \
  libxcb-xfixes0-dev libxcb-xinput-dev libxcb-composite0-dev \
  libxcb-ewmh-dev libxcb-icccm4-dev libxcb-res0-dev libxcb-errors-dev \
  libffi-dev libexpat1-dev libxml2-dev libliftoff-dev cmake g++-14 \
  libtomlplusplus-dev hwdata
```

## Dependencies built from source

The following must be built from source because Ubuntu 24.04 packages are too old.
Build them **in this order** (each depends on the previous). Clone each repo,
check out the listed tag, and build/install using the project's standard method
(cmake or meson). Run `sudo ldconfig` after each install.

| # | Repository | Tag | Build system | Notes |
|---|-----------|-----|-------------|-------|
| 1 | [hyprutils](https://github.com/hyprwm/hyprutils) | `v0.10.4` | cmake | Needs GCC 14+ (`-DCMAKE_CXX_COMPILER=g++-14`) |
| 2 | [hyprlang](https://github.com/hyprwm/hyprlang) | `v0.6.8` | cmake | Needs GCC 14+ |
| 3 | [hyprcursor](https://github.com/hyprwm/hyprcursor) | `v0.1.13` | cmake | Needs GCC 14+ |
| 4 | [wayland](https://gitlab.freedesktop.org/wayland/wayland) | `1.25.0` | meson | `-Ddocumentation=false -Dtests=false` |
| 5 | [wayland-protocols](https://gitlab.freedesktop.org/wayland/wayland-protocols) | `1.48` | meson | |
| 6 | [libdrm](https://gitlab.freedesktop.org/mesa/drm) | `libdrm-2.4.131` | meson | |
| 7 | [pixman](https://gitlab.freedesktop.org/pixman/pixman) | `pixman-0.46.4` | meson | |
| 8 | [libdisplay-info](https://gitlab.freedesktop.org/emersion/libdisplay-info) | `0.3.0` | meson | |
| 9 | [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots) | `0.20.0` | meson | `-Dexamples=false`; verify drm-backend is YES |

## Building CwC

```bash
cd path/to/cwcwm
meson setup build
ninja -C build
sudo ninja -C build install
```

CwC should now be available in your display manager or by running `cwc` from a TTY.

## Ubuntu 24.04

Building on Ubuntu 24.04 LTS may require building more system packages from source - if
something is missing when building above depencies, you are on your own to attempt
to build from source.
