Creating/Modifying Archives
===========================

The `arc extract` and `arc pack` commands can be used to extract, create and
modify .arc archives.

### Extracting an Archive

Run the following command to extract an archive,

    elf arc extract -o out mes.arc

This will extract the contents of the archive `mes.arc` into the `out`
directory. If the archive contains .mes files, they will be automatically
decompiled. This can be prevented by passing the `--raw` option.

### Packing an archive

In order to pack an archive, you must first create a manifest file listing
the files to be included in the archive. A manifest might look like this:

    #ARCPACK --mod-arc=MES.ARC.IN
    MES.ARC
    START.MES
    OPENIN.MES

This manifest instructs the `arc pack` command to:

1. Open the input archive `MES.ARC.IN`
2. Replace the files `START.MES` and `OPENIN.MES` with files by the same name
   on the filesystem
3. Write the output archive to `MES.ARC`

If the `--mod-arc` option is not specified, a new archive will be created
rather than modifying an existing archive.

The following command packs the archive:

    elf arc pack --game=isaku mes.manifest

(You should change the `--game` option to match your target game.)
