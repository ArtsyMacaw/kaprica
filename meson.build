project(
  'kaprica',
  'c',
  license: 'GPLv3',
  default_options:
  [
    'c_std=c11',
    'werror=true'
  ]
)

add_project_arguments(
  [
    '-Wno-unused-parameter',
    '-Wno-unused-result',
    '-Wno-missing-braces'
  ],
  language: 'c'
)

cc = meson.get_compiler('c')

sql = dependency('sqlite3', version: '>=3.6.19')
wayland = dependency('wayland-client')
wayland_scanner = find_program('wayland-scanner')
magic = cc.find_library('libmagic')

wlr_prot = 'protocol/wlr-data-control-unstable-v1.xml'
wlr_data = 'wlr-data-control'

header = custom_target(wlr_data + ' client header',
  input: wlr_prot,
  output: wlr_data + '.h',
  command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@']
)

glue = custom_target(wlr_data + ' code',
  input: wlr_prot,
  output: wlr_data + '.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@']
)

lib = static_library(
  wlr_data,
  glue,
  header,
  'clipboard.h',
  'copy.c',
  'paste.c',
  'database.c',
  'database.h',
  'xmalloc.h',
  'xmalloc.c',
  dependencies: [wayland, sql, magic]
)

executable('kapricad', 'kapricad.c', link_with: lib, dependencies: wayland)
executable('kaprica', 'kaprica.c', link_with: lib, dependencies: wayland)
