project('nulib', 'c',
  default_options : ['c_std=c11', 'default_library=static'])
add_project_arguments('-D_DEFAULT_SOURCE', language : 'c')

nulib_sources = [
  'src/buffer.c',
  'src/command.c',
  'src/file.c',
  'src/hashset.c',
  'src/lzss.c',
  'src/port.c',
  'src/sds.c',
  'src/system.c',
  'src/utfsjis.c'
]

inc = include_directories('include')

nulib = library('nulib', nulib_sources, include_directories : inc)

nulib_dep = declare_dependency(include_directories : inc, link_with : nulib)
