project('elf-tools', 'c', default_options : ['c_std=c11'])
add_project_arguments('-D_DEFAULT_SOURCE', language : 'c')

nulib_proj = subproject('nulib')
nulib_dep = nulib_proj.get_variable('nulib_dep')

incdirs = include_directories('include')

sources = [
  'src/core/arc.c',
  'src/core/mes/decompile.c',
  'src/core/mes/parse.c',
  'src/core/mes/print.c',
  'src/cli/arc_extract.c',
  'src/cli/arc_list.c',
  'src/cli/main.c',
  'src/cli/mes_decompile.c'
]

executable('elf', sources,
  dependencies : nulib_dep,
  c_args : ['-Wno-unused-parameter'],
  include_directories : incdirs,
  install : true)
