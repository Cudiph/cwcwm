{
  lib,
  stdenv,
  wayland,
  wlroots_0_19,
  hyprcursor,
  cairo,
  libxkbcommon,
  libinput,
  xxHash,
  luajit,
  luajitPackages,
  xwayland,
  libxcb,
  libxcb-wm,
  meson,
  ninja,
  pkg-config,
  wayland-protocols,
  wayland-scanner,
  git,
  libdrm,
  python3Minimal,
}:
stdenv.mkDerivation {
  pname = "cwcwm";
  version = "nightly";

  src = builtins.path {
    path = ./.;
    name = "source";
  };

  nativeBuildInputs = [
    meson
    ninja
    pkg-config
    wayland-protocols
    wayland-scanner
    git
    python3Minimal
  ];

  buildInputs = [
    wayland
    wlroots_0_19
    hyprcursor
    cairo
    libxkbcommon
    libinput
    xxHash
    xwayland
    libxcb
    libxcb-wm
    luajit
    luajitPackages.lgi
    libdrm
  ];

  meta = {
    mainProgram = "cwc";
    description = "Hackable wayland compositor";
    homepage = "https://github.com/Cudiph/cwcwm";
    license = lib.licenses.gpl3Plus;
    maintainers = [];
    platforms = lib.platforms.linux;
  };
}
