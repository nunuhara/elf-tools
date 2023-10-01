project('elf-tools', 'c', default_options : ['c_std=c11'])
add_project_arguments('-D_DEFAULT_SOURCE', language : 'c')

nulib_proj = subproject('nulib')
nulib_dep = nulib_proj.get_variable('nulib_dep')

flex = find_program('flex')
bison = find_program('bison')

flexgen = generator(flex,
                    output : '@BASENAME@.yy.c',
                    arguments : ['-o', '@OUTPUT@', '@INPUT@'])

bisongen = generator(bison,
                     output : ['@BASENAME@.tab.c', '@BASENAME@.tab.h'],
                     arguments : ['--verbose', '--debug', '-Wcounterexamples', '@INPUT@', '--defines=@OUTPUT1@', '--output=@OUTPUT0@'])

incdirs = include_directories('include')

sources = [
  'src/core/arc/open.c',
  'src/core/mes/decompile.c',
  'src/core/mes/flat_parser.c',
  'src/core/mes/pack.c',
  'src/core/mes/parse.c',
  'src/core/mes/print.c',
  'src/core/mes/system.c',
  'src/cli/arc_extract.c',
  'src/cli/arc_list.c',
  'src/cli/arc_pack.c',
  'src/cli/lzss_compress.c',
  'src/cli/lzss_decompress.c',
  'src/cli/main.c',
  'src/cli/mes_compile.c',
  'src/cli/mes_decompile.c'
]

sources += flexgen.process('src/core/mes/flat_lexer.l')
sources += bisongen.process('src/core/mes/flat_parser.y')

sources += flexgen.process('src/core/arc/manifest_lexer.l')
sources += bisongen.process('src/core/arc/manifest_parser.y')

executable('elf', sources,
  dependencies : nulib_dep,
  c_args : ['-Wno-unused-parameter'],
  include_directories : incdirs,
  install : true)
