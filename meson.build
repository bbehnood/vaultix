project(
  'vaultix',
  'c',
  default_options: [
    'c_std=c11',
    'warning_level=3',
  ]
)

add_project_arguments(
  '-D_POSIX_C_SOURCE=200809L',
  language: 'c',
)

src = files(
  'src/main.c',
  'src/server.c',
  'src/signals.c'
)

executable(
  'vaultix',
  src,
)
