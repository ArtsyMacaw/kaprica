project(
  'kaprica',
  'c',
  license: 'GPLv3',
  version: 'Pre-release',
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

# Build the executables
subdir('src')

# Install systemd user service file
systemd = dependency('systemd', required: get_option('systemd'))
if systemd.found()
  systemd_user_unit_dir = systemd.get_pkgconfig_variable('systemduserunitdir')
  install_data(
    'kaprica.service',
    install_dir: '@0@'.format(systemd_user_unit_dir)
  )
endif

# Generate man pages if scdoc is available
scdoc = dependency('scdoc', native: true, required: get_option('man-pages'))
if scdoc.found()
  scdoc_prog = find_program(scdoc.get_pkgconfig_variable('scdoc'), native: true)
  sh = find_program('sh', native: true)
  mandir = get_option('mandir')
  man_files = [
            'doc/kapd.1.scd',
            'doc/kapd.5.scd',
            'doc/kapc.1.scd',
            'doc/kapg.1.scd',
  ]
  foreach filename : man_files
    topic = filename.split('.')[-3].split('/')[-1]
    section = filename.split('.')[-2]
    output = '@0@.@1@'.format(topic, section)

    custom_target(
      output,
      input: filename,
      output: output,
      command: [
        sh, '-c', '@0@ < @INPUT@ > @1@'.format(scdoc_prog.path(), output)
      ],
      install: true,
      install_dir: '@0@/man@1@'.format(mandir, section)
    )
  endforeach
endif
