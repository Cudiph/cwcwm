project('cwcwm', 'c', 'cpp', default_options: ['c_std=gnu23,gnu17,gnu11','b_ndebug=if-release'])

cwc_datadir = get_option('datadir') / 'cwc'
add_project_arguments([
  '-DWLR_USE_UNSTABLE',
  '-DHHMAP_AUTOMATIC_REHASH',
  '-DCWC_PRIVATE=',

  '-DCWC_VERSION="v0.2.0-dev"',
  '-DCWC_DATADIR="' + cwc_datadir + '"',
], language: 'c')

git_version_hash = run_command(['git', 'rev-parse', '--short', 'HEAD'], capture: true).stdout().strip()
if git_version_hash != ''
  add_project_arguments(['-DCWC_GITHASH="' + git_version_hash + '"'], language: 'c')
endif

cc = meson.get_compiler('c')
proj_dir = meson.current_source_dir()

# dependencies (required)
wayland_server = dependency('wayland-server')
hyprcursor = dependency('hyprcursor')
cairo = dependency('cairo')
xkbcommon = dependency('xkbcommon')
libinput = dependency('libinput')
xxhash = dependency('libxxhash')
lua = dependency('luajit')
xcb = dependency('xcb')
libdrm_header = dependency('libdrm').partial_dependency(compile_args: true, includes: true)
libm = cc.find_library('m', required : true)
libdl = cc.find_library('dl', required : true)

# wlroots as subproject, if any
wlroots_version = ['>=0.19.0', '<0.20.0']
subproject(
	'wlroots',
	default_options: ['examples=false'],
	required: false,
	version: wlroots_version,
)
wlr = dependency('wlroots-0.19', version: wlroots_version, fallback: 'wlroots')

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
  libdrm_header,
  libm,
  libdl,
]

cwc_inc = include_directories('include')

# protocols
wlr_files = []
subdir('protocol')

subdir('src')
subdir('cwctl')

if get_option('plugins')
  subdir('plugins')
endif

if get_option('tests')
  subdir('tests')
endif

# install script
install_subdir('lib', install_dir: cwc_datadir)
install_subdir('defconfig', install_dir: cwc_datadir)
install_subdir('doc', install_dir: get_option('datadir') / 'doc/cwc')
install_subdir('include/cwc', install_dir: get_option('includedir'))
install_data('cwc.desktop', install_dir: get_option('datadir') / 'wayland-sessions')
install_data('cwc-portals.conf', install_dir: get_option('datadir') / 'xdg-desktop-portal')
