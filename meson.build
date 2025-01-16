project('cwcwm', 'c', 'cpp', default_options: ['c_std=gnu23,gnu17,gnu11','b_ndebug=if-release'])

add_project_arguments(['-DWLR_USE_UNSTABLE'], language: 'c')
add_project_arguments(['-DHHMAP_AUTOMATIC_REHASH'], language: 'c')

# versioning
git_version_hash = run_command(['git', 'rev-parse', '--short', 'HEAD'], capture: true)
add_project_arguments(['-DCWC_VERSION="0.0.1"', '-DCWC_GITHASH="'+git_version_hash.stdout().strip()+'"'], language: 'c')

cwc_datadir = '/usr/share/cwc'
add_project_arguments(['-DCWC_DATADIR="' + cwc_datadir + '"'], language: 'c')

cc = meson.get_compiler('c')
proj_dir = meson.current_source_dir()

# dependencies (required)
wlr = dependency('wlroots-0.19')
wayland_server = dependency('wayland-server')
hyprcursor = dependency('hyprcursor')
cairo = dependency('cairo')
xkbcommon = dependency('xkbcommon')
libinput = dependency('libinput')
xxhash = dependency('libxxhash')
lua = dependency('luajit')
xcb = dependency('xcb')
libm = cc.find_library('m', required : true)
libdl = cc.find_library('dl', required : true)

# # for testing custom wlroots commits
# wlr_inc = include_directories('./wlroots/include/')
# wlr_lib = cc.find_library('wlroots-0.19', required : true, dirs : proj_dir / 'wlroots/build')
# wlr = declare_dependency(dependencies: wlr_lib, include_directories: [wlr_inc, '/usr/include/libdrm'])


all_deps = [
  wayland_server,
  wlr,
  hyprcursor,
  cairo,
  xkbcommon,
  libinput,
  xxhash,
  lua,
  xcb,
  libm,
  libdl,
]

cwc_inc = include_directories('include')

# protocols
wlr_files = []
subdir('protocol')

subdir('src')

# run make header first to create include folder
if get_option('plugins')
  subdir('plugins')
endif

if get_option('tests')
  subdir('tests')
endif

# install script
install_subdir('lib', install_dir: cwc_datadir)
install_subdir('defconfig', install_dir: cwc_datadir)
install_subdir('doc', install_dir: '/usr/share/doc/cwc')
install_subdir('include/cwc', install_dir: '/usr/include')
install_data('cwc.desktop', install_dir: '/usr/share/wayland-sessions')
install_data('cwc-portals.conf', install_dir: '/usr/share/xdg-desktop-portal')
