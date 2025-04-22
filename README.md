# Indexer

**Program to search for words (terms) in text documents**  
Builds an in-memory index of terms from a given directory of files, providing a command line interface to search for words across all parsed documents.

---

## Usage & Arguments

```
./<exec> <data-dir> [--help --type <1...n> --limit <n> --stderr <fpath> --outfile <fpath>]
```

Where `<exec>` is the path to your executable file.

Note that the position of the optional arguments (within `[..]`) does not matter. For example, `--limit 10 --type txt md` is equivalent to `--type txt md --limit 10`, and both are valid.

### Required, Positional Arguments

#### `<data-dir>`: path to directory of files to index

- May contain the files directly, or as subdirectories. All subdirectories are checked.
- Example: `data/enwiki/`

### Optional Arguments / Flags

#### `--help`

- Print info on arguments, ie. a short variant of this README section

#### `--type <1...n>`: filter included data files by extension

- 1..n file extensions to include from `data-dir`, separated by spaces. Do not include the leading dot.
- If this is not present as an argument, **all** files are included, even hidden and system files.
- Example 1: `--type txt` - include plaintext files
- Example 2: `--type txt html md` - include plaintets, html and markdown files
- Invalid: `--type .txt`

#### `--limit <n>`: limit number of included data files

- If this argument is present, sets a hard limit on the number of included data files.
- Primarily intented to be used for development, to avoid parsing 100k documents just to check if things work.
- Example: `--limit 100` - stop parsing at 100 files

#### `--outfile <fpath>`: log succesful queries/results to a file

- Example: `--outfile log/results.log`
- If the given directory/file does not exist, they will be created. Otherwise, the program will append to the file.

#### `--stderr <fpath | tty>`: redirect stderr to file, or another terminal

- All prints done by 'printing.h' are done to the `stderr` stream. This means you can organize your output, keeping the command line interface clean while also getting a log of what's happening through e.g. debug prints.
- I recommend turning off colors for printing.h if you utilize this for a file
- Be a bit careful with redirecting to a file, as it will be overwritten by the program without warning. Another terminal window is better.
- Example 1: `--stderr log/stderr.log` - stderr to file
  - `log/` directory will be created if it does not exist.
  - The file will be overwritten every time you run the program
- Example 2: `--stderr /dev/ttys003` -
  - redirects stderr to another terminal
  - Tip: enter `tty` in a terminal to get its identifier

### Piped Input

In addition to runtime arguments, the program also supports _piped_ input, which it will treat as queries for the program once the indexing is completed.

**Example using `echo`**: `echo "(a && b) &! c" | ./<exec> <...args>`.

**Example using `cat`**; assuming you have a file named `my_queries.txt` with the following:

```
(a && b) &! c
x && y && z
```

You can then do `cat my_queries.txt | ./<exec> <...args>` to run the two queries above before exiting.

---

## Compiling the program

A makefile is provided to simplify compiling the program. The makefile provides two configurations for compiling the program, _debug_ and _release_.

### _debug_

Compile with `make` or `make exec`.  
By default, the makefile is set to enable compile-time warnings and symbolic information nescessary for program such as gdb and lldb.
This mode is default for a reason. Unless you are certain the program works as intended, utilize this option.

### _release_

If `DEBUG=0` is set in the makefile, or overridden by `make DEBUG=0`, the program will instead be compiled with minimal warnings and a high degree of optimization.  
The compiled executable will be placed in `bin/release` and object dependencies will be placed in `obj/`.

Building in release-mode also directs the compiler to completely remove:

- All `printing.h` invocations except for `pr_error` and `PANIC`
- All assertions, either through `assert.h` or `printing.h`

---

## Abstract Data Types (ADTs)

The interfaces at `include/adt/` specify how a structure should _**behave**_; they do not specify how that interface should be _**implemented**_, enabling a great degree of flexibility, creativity and experimentation.  
A map, for example, is typically implemented with a hashmap, but in some situations a BST based map could perform better. We encourage (or perhaps expect) you to try out different implementations for the specified APIs.

**For each distinct interface in the `include/adt/` folder, specify one implementation of that interface to utilize for the compiled application in the Makefile.**  
You can (and should) change this during your development to see how different implementations of the same ADT performs.

---

## Included Data Archive (`data/enwiki.zip`)

**You should extract `enwiki.zip` and unzip the archive only after reading this section**.

The entire dataset of english wikipedia articles consists of around 7M documents, of which we have provided 1M articles as plaintext documents for you to test your program with. You are not required to index them all (hence the `--limit <n>` argument). Rather; they are provided for your convenience, as it can be troublesome to find large datasets of files suitable for indexing.

In order to avoid your IDE becoming sluggish from parsing all these text files, or in worst case crashing entirely, you should configure it to ignore these data files.

For **Visual Studio Code**, follow this step by step to do this.

1. Open the vscode command palette (Typically done by pressing the `f1` key)
2. Enter `>Preferences: Open User Settings (JSON)` (including the leading `>`) and submit
3. A `JSON` file will open. This is your personal vscode settings, and directly map to the settings you may have enabled through the settings GUI.
4. Append the following to your `files.watcherExclude` and `search.exclude` entries. These entries very likely exist, but create them if they don't.

```json
{
  "files.watcherExclude": {
    "**/data/**": true
    /* ... */
  },
  "search.exclude": {
    "**/data/**": true
    /* ... */
  }
}
```

5. Your IDE now ignores these directories, and you may extract the archive at its current location.
