project(
  'ivm', 'c',
  version: '0.1',
)
add_global_arguments('-Wno-dollar-in-identifier-extension', language : 'c')
add_global_arguments('-Wno-unused-parameter', language : 'c')
add_global_arguments('-Wno-language-extension-token', language : 'c')
add_global_arguments('-Wno-unused-function', language : 'c')
add_global_arguments('-Wgnu-statement-expression-from-macro-expansion', language : 'c')
add_global_arguments('-O3', language : 'c')

incdir = include_directories('include')


subdir('src')

raylib = dependency('raylib')
glew = dependency('glew')

cc = meson.get_compiler('c')
math = cc.find_library('m', required : true)

executable(
  'ivm',
  vm_files,
  include_directories : incdir,
  dependencies: [raylib, glew, math],
  install : true
)

executable(
  'iasm',
  asm_files,
  include_directories : incdir,
  dependencies: [math],
  install : true
)

executable(
  'ivm-objdump',
  common_sources + 'src/ivm-objdump/main.c',
  include_directories : incdir,
  install : true
)
