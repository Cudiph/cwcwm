assets = {
  'client': 'script/client.lua',
  'screen': 'script/screen.lua',
  'binds': 'script/binds.lua',
  'plugin': 'script/plugin.lua',
}

script_assets = []
foreach _, path : assets
  script_assets += path
endforeach

script_header = custom_target(
  name.underscorify() + '_h',
  input: script_assets,
  output: 'script-asset.h',
  command: ['python', meson.current_source_dir() / 'gen.py', '@INPUT@', '@OUTPUT@'],
)

srcs = [
  'main.c',
  'ipc-common.c',
  'cmd-screen.c',

  script_header,
]

executable(
  'cwctl',
  srcs,
  include_directories : cwc_inc,
  install: true,
  install_dir: get_option('bindir'),
)
