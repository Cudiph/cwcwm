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
  xwayland,
  libxcb,
  libxcb-wm,
  gnumake,
  meson,
  ninja,
  pkg-config,
  wayland-protocols,
  wayland-scanner,
  git,
  libdrm,
  python3Minimal,
  boost,
  makeWrapper,
}: let
  luaEnv = luajit.withPackages (ps: [
    ps.lgi
  ]);
in
  stdenv.mkDerivation {
    pname = "cwc";
    version = "nightly";

    src = builtins.path {
      path = ../.;
      name = "source";
    };

    nativeBuildInputs = [
      gnumake
      meson
      ninja
      pkg-config
      wayland-protocols
      wayland-scanner
      git
      python3Minimal
      boost
      makeWrapper
      luaEnv
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
      luaEnv
      libdrm
    ];

    mesonFlags = ["-Dplugins=true" "-Dtests=true"];

    LUA_CPATH = "${luaEnv}/lib/lua/${luajit.luaversion}/?.so";
    LUA_PATH = "${luaEnv}/share/lua/${luajit.luaversion}/?.lua;;";

    postInstall = ''
      # Very long LUA_PATH, maybe fixable

      wrapProgram "$out/bin/cwc" \
      --set LUA_PATH '${luaEnv}/share/lua/5.1/?.lua;${luaEnv}/share/lua/5.1/?/init.lua;${placeholder "out"}/share/cwc/lib/?.lua;${placeholder "out"}/share/cwc/lib/?/init.lua' \
      --set LUA_CPATH '${luaEnv}/lib/lua/5.1/?.so' \
      --set CWC_PLUGIN_PATH "$out/share/cwc/plugins"
    '';

    passthru = {inherit luajit;};
    meta = {
      mainProgram = "cwc";
      description = "Hackable wayland compositor";
      homepage = "https://github.com/Cudiph/cwcwm";
      license = lib.licenses.gpl3Plus;
      maintainers = [];
      platforms = lib.platforms.linux;
    };
  }
