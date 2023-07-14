elf-tools
=========

This is a collection of command-line tools for viewing and editing file formats
used in elf/Silky's games.

Current Status
--------------

I am currently building these tools for the purpose of reverse engineering the
AI5WIN engine. As such, I am prioritizing the compiler/decompiler for that
engine over other tools/features that might be more useful for translation,
extracting assets, etc.

Game Support
------------

Only the games listed below have been tested. The string listed under "Option
Name" should be given using the `--game` command when working with that game's
files (e.g. `arc extract --game=doukyuusei -o out doukyuusei/mes.arc`).

| Option Name | Game                                             | Status                        |
| ----------- | ------------------------------------------------ | ----------------------------- |
| aishimai    | 愛姉妹 ～二人の果実～                            | OK                            |
| beyond      | ビ・ ヨンド ～黒大将に見られてる～               | Several files don't decompile |
| doukyuusei  | 同級生 Windows版                                 | One file doesn't decompile    |
| isaku       | 遺作 リニューアル                                | OK                            |
| koihime     | 恋姫                                             | OK                            |
| yukinojou   | あしたの雪之丞                                   | OK                            |
| yuno        | この世の果てで恋を唄う少女YU-NO (エルフclassics) | OK                            |

Building
--------

First install the dependencies:

* meson
* ninja

Then build the tools with meson,

    mkdir build
    meson build
    ninja -C build

Usage
-----

All of the tools are accessed through the single `elf` executable. Running
`elf` or any command without arguments will print the relevant usage
instructions. E.g.

    elf
    elf mes
    elf mes decompile

The currently implemented commands are:

    elf arc extract   - Extract an archive file
    elf arc list      - List the contents of an archive file
    elf mes decompile - Decompile a .mes file

Source Code
-----------

The source code is available [on github](https://github.com/nunuhara/elf-tools).
