elf-tools
=========

This is a collection of command-line tools for viewing and editing file formats
used in elf/Silky's games.

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
    elf arc pack      - Create/modify an archive file
    elf cg  convert   - Convert an image file to another format
    elf mes compile   - Compile a .mes file
    elf mes decompile - Decompile a .mes file

### How-To

[Text Replacement](README-text.md)  
[Archive Extraction/Creation/Modification](README-arc.md)

Source Code
-----------

The source code is available [on github](https://github.com/nunuhara/elf-tools).
