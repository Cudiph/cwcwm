assets = {
  'client': 'script/client.lua',
  'screen': 'script/screen.lua',
  'binds': 'script/binds.lua',
}

assets_code = {}
foreach name, path : assets
  code = custom_target(
    name.underscorify() + '_h',
    input: path,
    output: '@BASENAME@-asset.h',
    command: ['python', meson.current_source_dir() / 'gen.py', '@INPUT@', '@OUTPUT@'],
  )

  assets_code += { name: code }
endforeach

srcs = [
  'main.c',
  'ipc-common.c',

  assets_code['client'],
  assets_code['screen'],
  assets_code['binds'],
]

executable(
  'cwctl',
  srcs,
  include_directories : cwc_inc,
  install: true,
  install_dir: '/usr/bin',
)
