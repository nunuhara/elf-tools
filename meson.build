project('elf-tools', 'c', 'cpp', default_options : ['c_std=c11'])
add_project_arguments('-D_DEFAULT_SOURCE', language : 'c')

libai5_proj = subproject('libai5')
libai5_dep = libai5_proj.get_variable('libai5_dep')

png = dependency('libpng')

flex = find_program('flex')
bison = find_program('bison')

tool_deps = [libai5_dep, png]

flexgen = generator(flex,
                    output : '@BASENAME@.yy.c',
                    arguments : ['-o', '@OUTPUT@', '@INPUT@'])

bisongen = generator(bison,
                     output : ['@BASENAME@.tab.c', '@BASENAME@.tab.h'],
                     arguments : ['--verbose', '--debug', '-Wcounterexamples', '@INPUT@', '--defines=@OUTPUT1@', '--output=@OUTPUT0@'])

incdirs = include_directories('include')

version_h = vcs_tag(fallback : 'unknown',
                    input : 'version.h.in',
                    output : 'version.h')
core_sources = [
  version_h,
  'src/core/a6.c',
  'src/core/anim/pack.c',
  'src/core/anim/render.c',
  'src/core/arc/arc.c',
  'src/core/map.c',
  'src/core/mdd.c',
  'src/core/mp3.c',
  'src/core/mes/ctor.c',
  'src/core/mes/decompile.c',
  'src/core/mes/flat_parser.c',
  'src/core/mes/pack.c',
  'src/core/mes/print.c',
  'src/core/mes/size.c',
  'src/core/mes/text_parser.c',
  'src/core/file.c',
]

core_sources += flexgen.process('src/core/mes/flat_lexer.l')
core_sources += bisongen.process('src/core/mes/flat_parser.y')

core_sources += flexgen.process('src/core/mes/aiw_flat_lexer.l')
core_sources += bisongen.process('src/core/mes/aiw_flat_parser.y')

core_sources += flexgen.process('src/core/arc/manifest_lexer.l')
core_sources += bisongen.process('src/core/arc/manifest_parser.y')

core_sources += flexgen.process('src/core/anim/anim_lexer.l')
core_sources += bisongen.process('src/core/anim/anim_parser.y')

libelf = static_library('elf', core_sources,
  dependencies : tool_deps,
  c_args : ['-Wno-unused-parameter'],
  include_directories : incdirs)

static_link_args = []
if host_machine.system() == 'windows'
  static_link_args = ['-static']
endif

cli_sources = [
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
  'src/cli/mdd_render.c',
  'src/cli/mes_compile.c',
  'src/cli/mes_decompile.c',
  'src/cli/mp3_extract.c',
  'src/cli/mp3_render.c',
  'src/cli/mpx_unpack.c',
  'src/cli/save.c',
]

executable('elf', cli_sources,
  dependencies : tool_deps,
  c_args : ['-Wno-unused-parameter'],
  link_args : static_link_args,
  link_with : libelf,
  include_directories : incdirs,
  install : true)

gui_sources = [
  'src/gui/basic_text_view.cpp',
  'src/gui/filesystem_view.cpp',
  'src/gui/game_dialog.cpp',
  'src/gui/gelf.cpp',
  'src/gui/mainwindow.cpp',
  'src/gui/navigator.cpp',
  'src/gui/navigator_model.cpp',
  'src/gui/navigator_node.cpp',
  'src/gui/navigator_view.cpp',
  'src/gui/player.cpp',
  'src/gui/player_controls.cpp',
  'src/gui/smes_view.cpp',
  'src/gui/syntax_highlighter.cpp',
  'src/gui/viewer.cpp',
]

gui_moc = [
  'src/gui/basic_text_view.hpp',
  'src/gui/filesystem_view.hpp',
  'src/gui/game_dialog.hpp',
  'src/gui/gelf.hpp',
  'src/gui/mainwindow.hpp',
  'src/gui/navigator.hpp',
  'src/gui/navigator_model.hpp',
  'src/gui/navigator_view.hpp',
  'src/gui/player.hpp',
  'src/gui/player_controls.hpp',
  'src/gui/smes_view.hpp',
  'src/gui/syntax_highlighter.hpp',
  'src/gui/viewer.hpp',
]

qt5 = import('qt5')
qt5_dep = dependency('qt5', modules : ['Core', 'Gui', 'Widgets', 'Multimedia', 'MultimediaWidgets'], required : false)

if qt5_dep.found()
  generated_files = qt5.preprocess(moc_headers : gui_moc, dependencies : [qt5_dep])

  # NOTE: static linking of Qt not supported by meson per
  #       https://github.com/mesonbuild/meson/pull/7733
  executable('gelf', gui_sources, generated_files,
    dependencies : [tool_deps, qt5_dep],
    link_with : libelf,
    include_directories : incdirs,
    gui_app : true,
    install : true)
endif
