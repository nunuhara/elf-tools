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

If the replacement text spans multiple lines, the lines are joined *without*
a newline in between. For example, the replacement text for:

    # 0 "1行目"
    First line
    continuation.

will become `First linecontinuation`. If you want your text to span multiple
lines, you need to add the correct amount of padding at the end of each line.
This can be automated by using the `columns` directive. For example,

    # columns = 14

    # 0 "1行目"
    First line
    continuation.

In this case, the replacement text would be `First line    continuation.`
The amount of padding required will depend on the game.

### Substituting Replacement Text

Once you have finished writing all of the replacement text, run the following
command to substitute the replacement text into the .mes file:

    elf mes compile -o out.mes --base in.mes -t in.txt

This should create a file named `out.mes` containing the replacement text.
