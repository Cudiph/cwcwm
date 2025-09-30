{
  lib,
  stdenv,
  wayland,
  wlroots_0_19,
  hyprcursor,
  pango,
  cairo,
  gdk-pixbuf,
  glib,
  libxkbcommon,
  libinput,
  xxHash,
  luajit,
  gobject-introspection,
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
  boost,
  makeWrapper,
  wrapGAppsHook,
  gtk3Support ? false,
  gtk3 ? null,
}:
assert gtk3Support -> gtk3 != null; let
  luaEnv = luajit.withPackages (ps: [
    ps.lgi
  ]);
  commonDeps =
    [
      gdk-pixbuf
      pango
      glib
    ]
    ++ lib.optional gtk3Support gtk3;
in
  stdenv.mkDerivation {
    pname = "cwc";
    version = "nightly";

    src = builtins.path {
      path = ../.;
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
      boost
      makeWrapper
      wrapGAppsHook
      gobject-introspection
    ];

    buildInputs =
      [
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
      ]
      ++ commonDeps;

    propagatedBuildInputs = commonDeps;

    doCheck = true;

    GI_TYPELIB_PATH = "${pango.out}/lib/girepository-1.0";

    mesonFlags = ["-Dplugins=true" "-Dtests=true"];

    LUA_CPATH = "${luaEnv}/lib/lua/${luajit.luaversion}/?.so";
    LUA_PATH = "${luaEnv}/share/lua/${luajit.luaversion}/?.lua;;";

    postInstall = ''
      # Copied from @SK4G from #31

      wrapProgram "$out/bin/cwc" \
      --prefix LUA_PATH : "${luaEnv}/share/lua/5.1/?.lua;${luaEnv}/share/lua/5.1/?/init.lua;${placeholder "out"}/share/cwc/?.lua" \
      --prefix LUA_CPATH : "${luaEnv}/lib/lua/5.1/?.so;${placeholder "out"}/lib/cwc/plugins/?.so" \
      --prefix GI_TYPELIB_PATH : "${lib.makeSearchPath "lib/girepository-1.0" [gobject-introspection gtk3 pango gdk-pixbuf]}" \
      --prefix XDG_DATA_DIRS : "$out/share:${gtk3}/share:${gdk-pixbuf}/share:${pango}/share" \
      --prefix GIO_MODULE_DIR : "${glib}/lib/gio/modules" \
    '';

    passthru = {
      providedSessions = ["cwc"];
      inherit luajit;
    };
    meta = {
      mainProgram = "cwc";
      description = "Hackable wayland compositor";
      homepage = "https://github.com/Cudiph/cwcwm";
      license = lib.licenses.gpl3Plus;
      maintainers = [];
      platforms = lib.platforms.linux;
    };
  }
