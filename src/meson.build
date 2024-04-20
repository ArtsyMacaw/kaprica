# Get generated glue + header code from protocol
subdir('protocol')

cc = meson.get_compiler('c')

# Dependencies
sql = dependency('sqlite3', version: '>=3.6.19')
wayland = dependency('wayland-client')
gtk = dependency('gtk4')
imagemagick = dependency('MagickWand')
inih = dependency('inih')
magic = cc.find_library('magic')
xxhash = cc.find_library('xxhash')

# Set defines
conf_data = configuration_data()
conf_data.set('PROJECT_VERSION', '"@0@"'.format(meson.project_version()))
configure_file(output: 'config.h', configuration: conf_data)

lib = static_library(
  wlr_data,
  glue,
  header,
  'clipboard.h',
  'clipboard.c',
  'source.c',
  'offer.c',
  'database.h',
  'database.c',
  'xmalloc.h',
  'xmalloc.c',
  'detection.h',
  'detection.c',
  'hash.h',
  dependencies: [wayland, sql, magic, gtk, imagemagick, xxhash, inih]
)

executable('kapd', 'kapricad.c', link_with: lib, install: true)
executable('kapc', 'kaprica.c', link_with: lib, install: true)
executable('kapg', 'kapricag.c', link_with: lib, dependencies: gtk, install: true)
