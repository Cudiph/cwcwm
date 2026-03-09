# Originally from https://github.com/NixOS/nixpkgs/blob/nixos-25.11/pkgs/development/libraries/wlroots/default.nix
# At the time of writing this, wlroots_0_20 is not present in nixpkgs (including unstable)
# MIT License notice:
# Copyright (c) 2003-2026 Eelco Dolstra and the Nixpkgs/NixOS contributors
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
{
  lib,
  stdenv,
  fetchFromGitLab,
  meson,
  ninja,
  pkg-config,
  wayland-scanner,
  libGL,
  wayland,
  wayland-protocols,
  libinput,
  libxkbcommon,
  pixman,
  libcap,
  libgbm,
  libxcb-wm,
  libxcb-render-util,
  libxcb-image,
  libxcb-errors,
  libx11,
  hwdata,
  seatd,
  vulkan-loader,
  glslang,
  libliftoff,
  libdisplay-info,
  lcms2,
  evdev-proto,
  nixosTests,
  testers,

  enableXWayland ? true,
  xwayland ? null,
}:

let
  generic =
    {
      version,
      hash,
      extraBuildInputs ? [ ],
      extraNativeBuildInputs ? [ ],
      patches ? [ ],
      postPatch ? "",
    }:
    stdenv.mkDerivation (finalAttrs: {
      pname = "wlroots";
      inherit version;

      inherit enableXWayland;

      src = fetchFromGitLab {
        domain = "gitlab.freedesktop.org";
        owner = "wlroots";
        repo = "wlroots";
        rev = finalAttrs.version;
        inherit hash;
      };

      inherit patches postPatch;

      # $out for the library and $examples for the example programs (in examples):
      outputs = [
        "out"
        "examples"
      ];

      strictDeps = true;
      depsBuildBuild = [ pkg-config ];

      nativeBuildInputs = [
        meson
        ninja
        pkg-config
        wayland-scanner
        glslang
        hwdata
      ]
      ++ extraNativeBuildInputs;

      propagatedBuildInputs = [
        # The headers of wlroots #include <libinput.h>, and consumers of `wlroots` need not add it explicitly, hence we propagate it.
        libinput
      ];

      buildInputs = [
        libliftoff
        libdisplay-info
        libGL
        libxkbcommon
        libgbm
        pixman
        seatd
        vulkan-loader
        wayland
        wayland-protocols
        libx11
        libxcb-errors
        libxcb-image
        libxcb-render-util
        libxcb-wm
      ]
      ++ lib.optional stdenv.hostPlatform.isLinux libcap
      ++ lib.optional stdenv.hostPlatform.isFreeBSD evdev-proto
      ++ lib.optional finalAttrs.enableXWayland xwayland
      ++ extraBuildInputs;

      mesonFlags = [
        (lib.mesonEnable "xwayland" finalAttrs.enableXWayland)
      ]
      # The other allocator, udmabuf, is a linux-specific API
      ++ lib.optionals (!stdenv.hostPlatform.isLinux) [
        (lib.mesonOption "allocators" "gbm")
      ];

      postFixup = ''
        # Install ALL example programs to $examples:
        # screencopy dmabuf-capture input-inhibitor layer-shell idle-inhibit idle
        # screenshot output-layout multi-pointer rotation tablet touch pointer
        # simple
        mkdir -p $examples/bin
        cd ./examples
        for binary in $(find . -executable -type f -printf '%P\n' | grep -vE '\.so'); do
          cp "$binary" "$examples/bin/wlroots-$binary"
        done
      '';

      # Test via TinyWL (the "minimum viable product" Wayland compositor based on wlroots):
      passthru.tests = {
        tinywl = nixosTests.tinywl;
        pkg-config = testers.hasPkgConfigModules {
          package = finalAttrs.finalPackage;
        };
      };

      meta = {
        description = "Modular Wayland compositor library";
        longDescription = ''
          Pluggable, composable, unopinionated modules for building a Wayland
          compositor; or about 50,000 lines of code you were going to write anyway.
        '';
        inherit (finalAttrs.src.meta) homepage;
        changelog = "https://gitlab.freedesktop.org/wlroots/wlroots/-/tags/${version}";
        license = lib.licenses.mit;
        platforms = lib.platforms.linux ++ lib.platforms.freebsd;
        maintainers = with lib.maintainers; [
          synthetica
          wineee
          doronbehar
        ];
        pkgConfigModules = [
          (
            if lib.versionOlder finalAttrs.version "0.18" then
              "wlroots"
            else
              "wlroots-${lib.versions.majorMinor finalAttrs.version}"
          )
        ];
      };
    });

in
generic {
  version = "0.20.0-rc4";
  hash = "sha256-LIS6FqWfkq36f62RYlkD5138mP/ZSTTOhw+ToPK5lzY=";
  extraBuildInputs = [
    lcms2
  ];
}
