project('elf-tools', 'c', default_options : ['c_std=c11'])
add_project_arguments('-D_DEFAULT_SOURCE', language : 'c')

libai5_proj = subproject('libai5')
libai5_dep = libai5_proj.get_variable('libai5_dep')

png = dependency('libpng')

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
  'src/core/anim/pack.c',
  'src/core/anim/render.c',
  'src/core/mes/ctor.c',
  'src/core/mes/decompile.c',
  'src/core/mes/flat_parser.c',
  'src/core/mes/pack.c',
  'src/core/mes/print.c',
  'src/core/mes/size.c',
  'src/core/mes/text_parser.c',
  'src/core/file.c',
  'src/cli/a6_decompile.c',
  'src/cli/anim_compile.c',
  'src/cli/anim_decompile.c',
  'src/cli/anim_render.c',
  'src/cli/arc_extract.c',
  'src/cli/arc_list.c',
  'src/cli/arc_pack.c',
  'src/cli/ccd_unpack.c',
  'src/cli/cg_convert.c',
  'src/cli/eve_unpack.c',
  'src/cli/font_extract.c',
  'src/cli/lzss_compress.c',
  'src/cli/lzss_decompress.c',
  'src/cli/main.c',
  'src/cli/mes_compile.c',
  'src/cli/mes_decompile.c',
  'src/cli/mpx_unpack.c'
]

sources += flexgen.process('src/core/mes/flat_lexer.l')
sources += bisongen.process('src/core/mes/flat_parser.y')

sources += flexgen.process('src/core/arc/manifest_lexer.l')
sources += bisongen.process('src/core/arc/manifest_parser.y')

sources += flexgen.process('src/core/anim/anim_lexer.l')
sources += bisongen.process('src/core/anim/anim_parser.y')

executable('elf', sources,
  dependencies : [libai5_dep, png],
  c_args : ['-Wno-unused-parameter'],
  include_directories : incdirs,
  install : true)
