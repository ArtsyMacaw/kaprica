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

subdir('src')
