project(
  'vaultix',
  'c',
  default_options: [
    'c_std=c11',
    'warning_level=3',
  ]
)

if get_option('buildtype') == 'debug'
  add_project_arguments(
    '-D_POSIX_C_SOURCE=200809L',
    '-DDEBUG',
    language: 'c',
  )
else
  add_project_arguments(
    '-D_POSIX_C_SOURCE=200809L',
    language: 'c',
  )
endif

src = files(
  'src/main.c',
  'src/server.c',
  'src/signals.c',
  'src/parser.c',
  'src/database.c',
  'src/reply.c'
)

inc = include_directories('src')

executable(
  'vaultix',
  src,
  include_directories: inc,
)
