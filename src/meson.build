srcs = [
  'main.c',
  'server.c',

  'config.c',
  'plugin.c',
  'signal.c',
  'util.c',
  'util-map.c',

  'luac.c',
  'luaclass.c',
  'luaobject.c',

  'desktop/idle.c',
  'desktop/layer_shell.c',
  'desktop/output.c',
  'desktop/session_lock.c',
  'desktop/toplevel.c',
  'desktop/transaction.c',

  'input/cursor.c',
  'input/manager.c',
  'input/keybinding.c',
  'input/keyboard.c',
  'input/seat.c',
  'input/switch.c',
  'input/tablet.c',
  'input/touch.c',

  'layout/bsp.c',
  'layout/master.c',
  'layout/container.c',

  'objects/client.c',
  'objects/container.c',
  'objects/screen.c',
  'objects/tag.c',
  'objects/input.c',

  # Stable upstream protocols
  protocols_server_header['xdg-shell'],
  protocols_server_header['tablet-v2'],

  # Staging upstream protocols
  protocols_server_header['cursor-shape-v1'],
  protocols_server_header['ext-image-capture-source-v1'],
  protocols_server_header['ext-image-copy-capture-v1'],
  protocols_server_header['tearing-control-v1'],

  # Unstable upstream protocols
  protocols_server_header['pointer-constraints-unstable-v1'],

  # Other protocols
  protocols_server_header['wlr-layer-shell-unstable-v1'],
  protocols_server_header['wlr-output-power-management-unstable-v1'],
]

executable(
  'cwc',
  srcs,
  install: true,
  install_dir: '/usr/bin',
  dependencies: all_deps,
  include_directories : cwc_inc,
  link_args : ['-Wl,--export-dynamic'],
)
