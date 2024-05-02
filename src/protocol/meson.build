# Build the wayland data control protocol
wayland_scanner = find_program('wayland-scanner', native: true)

wlr_prot = 'wlr-data-control-unstable-v1.xml'
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
