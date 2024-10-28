project(
  'ivm', 'c',
  version: '0.1',
  default_options: ['warning_level=3']
)

incdir = include_directories('include')


subdir('src')

glfw = dependency('glfw3')
glew = dependency('glew')

cc = meson.get_compiler('c')
math = cc.find_library('m', required : false)

executable(
  'ivm',
  vm_files,
  include_directories : incdir,
  dependencies: [glfw, glew, math],
  install : true
)

executable(
  'iasm',
  asm_files,
  include_directories : incdir,
  dependencies: [math],
  install : true
)
