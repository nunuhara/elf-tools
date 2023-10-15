Text Replacement
================

The `mes decompile` and `mes compile` commands have text extraction and
substitution modes which can be used for translation purposes.

### Extracting Text

Run the following command to extract text from a .mes file:

    elf mes decompile -t -o in.txt in.mes

This will create a file `in.txt` which looks something like this

    # 0 "1行目"

    # 1 "2行目"

The number is the index of the target text in the .mes file. The string in
quotes is the original text.

### Adding Replacement Text

Beneath each target line, you should write the replacement text. Ensure that
there is an empty line between the replacement text and the following line.
For example,

    # 0 "1行目"
    First line.

    # 1 "2行目"
    Second line.

You can configure the allowed number of columns using the `columns` directive.
This way, a warning will be printed if a line exceeds the configured number of
columns. Using the `columns` directive also allows for one additional column
to be used per line.

For example, the size of the dialog box in Isaku (Renewal version) is 59
columns. So you would add this line to the top of each file:

    # columns = 59

### Substituting Replacement Text

Once you have finished writing all of the replacement text, run the following
command to substitute the replacement text into the .mes file:

    elf mes compile -o out.mes --base in.mes -t in.txt

This should create a file named `out.mes` containing the replacement text.
