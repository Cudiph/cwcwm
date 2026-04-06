# Building on Ubuntu / Pop!_OS

This guide documents the full build process for CwC on Ubuntu 24.04 / Pop!_OS,
including all dependencies that need to be built from source due to these distros
shipping older versions.

An experimental automated build script is also available at [`scripts/build-deps.sh`](../scripts/build-deps.sh).

## System packages

```bash
sudo apt install meson ninja-build wayland-protocols libwayland-dev \
  libcairo2-dev libxkbcommon-dev libinput-dev libxxhash-dev \
  libluajit-5.1-dev libxcb1-dev xwayland libdrm-dev lua-lgi \
  libpango1.0-dev gir1.2-pango-1.0 libzip-dev librsvg2-dev \
  libseat-dev libudev-dev libgbm-dev libgles-dev libegl-dev \
  libvulkan-dev glslang-tools liblcms2-dev libxcb-dri3-dev \
  libxcb-present-dev libxcb-render-util0-dev libxcb-shm0-dev \
  libxcb-xfixes0-dev libxcb-xinput-dev libxcb-composite0-dev \
  libxcb-ewmh-dev libxcb-icccm4-dev libxcb-res0-dev libxcb-errors-dev \
  libffi-dev libexpat1-dev libxml2-dev libliftoff-dev cmake g++-14
```

## Dependencies built from source

The following must be built from source because Ubuntu 24.04 packages are too old.
Build them **in this order** (each depends on the previous).

### 1. hyprutils (>= 0.7.1)

Required by hyprlang. Needs GCC 14 for C++23 support. Use an older tag to avoid
needing GCC 15.

```bash
git clone https://github.com/hyprwm/hyprutils
cd hyprutils
git checkout v0.7.1   # newer tags may require GCC 15+
cmake -B build -DCMAKE_CXX_COMPILER=g++-14
cmake --build build
sudo cmake --install build
sudo ldconfig
```

### 2. hyprlang (>= 0.4.2)

Required by hyprcursor.

```bash
git clone https://github.com/hyprwm/hyprlang
cd hyprlang
cmake -B build -DCMAKE_CXX_COMPILER=g++-14
cmake --build build
sudo cmake --install build
sudo ldconfig
```

### 3. hyprcursor

Required by CwC.

```bash
git clone https://github.com/hyprwm/hyprcursor
cd hyprcursor
cmake -B build -DCMAKE_CXX_COMPILER=g++-14
cmake --build build
sudo cmake --install build
sudo ldconfig
```

### 4. wayland (>= 1.24.0)

Required by wlroots 0.20.

```bash
git clone https://gitlab.freedesktop.org/wayland/wayland.git
cd wayland
git checkout 1.25.0
meson setup build -Ddocumentation=false -Dtests=false
ninja -C build
sudo ninja -C build install
sudo ldconfig
```

### 5. wayland-protocols (>= 1.38)

Required by wlroots 0.20 for protocol enum headers.

```bash
git clone https://gitlab.freedesktop.org/wayland/wayland-protocols.git
cd wayland-protocols
git checkout 1.48
meson setup build
ninja -C build
sudo ninja -C build install
sudo ldconfig
```

### 6. libdrm (>= 2.4.129)

Required by wlroots 0.20.

```bash
git clone https://gitlab.freedesktop.org/mesa/drm.git
cd drm
git checkout libdrm-2.4.131
meson setup build
ninja -C build
sudo ninja -C build install
sudo ldconfig
```

### 7. pixman (>= 0.43.0)

Required by wlroots 0.20.

```bash
git clone https://gitlab.freedesktop.org/pixman/pixman.git
cd pixman
git checkout pixman-0.43.0
meson setup build
ninja -C build
sudo ninja -C build install
sudo ldconfig
```

### 8. libdisplay-info (>= 0.2.0)

Required by wlroots 0.20 for DRM backend support.

```bash
git clone https://gitlab.freedesktop.org/emersion/libdisplay-info.git
cd libdisplay-info
git checkout 0.2.0
meson setup build
ninja -C build
sudo ninja -C build install
sudo ldconfig
```

### 9. wlroots (0.20.x)

```bash
git clone https://gitlab.freedesktop.org/wlroots/wlroots.git
cd wlroots
git checkout 0.20.0
meson setup build -Dexamples=false
ninja -C build
sudo ninja -C build install
sudo ldconfig
```

Verify all features are YES (especially drm-backend).

## Building CwC

```bash
cd cwcwm
meson setup build
ninja -C build
sudo ninja -C build install
```

CwC should now be available in your display manager or by running `cwc` from a TTY.
