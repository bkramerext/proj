# proj

Fast XML, JSON, log, CSV, and TSV queries from the command line.

`proj` reads structured or semi-structured text from standard input or a file and supports selection, renaming, filtering, sorting, grouping, aggregation, and joins, producing CSV output. Input format is detected automatically; it is not necessary to specify whether the input is XML, JSON, a Log4j-style log file, or CSV/TSV.

## Build

Requires a C++11-capable compiler (clang or g++).

```bash
# macOS: clang is expected to be preinstalled.
# Ubuntu:
sudo apt-get update
sudo apt-get install clang libc++-dev

make release
cp bin/proj /usr/local/bin   # or: make deploy
```

## Quickstart

A minimal query extracting one field from an XML document:

```bash
$ echo "<greeting>Hello World!</greeting>" | proj greeting
greeting
Hello World!
```

Grouping and aggregating CSV data:

```bash
$ printf 'name,dept,salary\nAda,Eng,95000\nGrace,Eng,102000\nLinus,Support,71000\n' \
  | proj dept 'sum[salary]'
dept,sum[salary]
Eng,197000.0
Support,71000.0
```

Filtering and sorting the same data:

```bash
$ printf 'name,dept,salary\nAda,Eng,95000\nGrace,Eng,102000\nLinus,Support,71000\n' \
  | proj name salary 'where[salary>80000]' 'sort[-salary]'
name,salary
Grace,102000
Ada,95000
```

Notes:
- Matching is case-insensitive by default, for field names, function names, and string comparisons.
- A non-aggregate column defines a group; aggregate functions (`sum`, `count`, `first`, and others) are computed per group.
- Directives such as `where[...]`, `sort[...]`, `top[n]`, `first[n]`, and `--distinct` do not produce an output column.
- Arguments containing `[`, `]`, `$`, or spaces should be quoted or escaped, since they are significant to both the shell and to `proj`.
- The same query language applies across JSON, XML, and CSV/TSV input, provided field names are consistent.

`proj` also supports left, inner, and outer joins (`join[path]`), reusable argument files (`@file`), custom column headers (`name:expr`), and pivoting. See the tutorial for examples of each.

## Learn more

- [11-step tutorial](tutorial/TUTORIAL.md) — intended to be run interactively from the `tutorial` directory.
- [Wiki](https://github.com/arlettedata/proj/wiki/Proj.--The-initial-wiki-entry.) — background and motivation.
- [tests/](tests) — example scenarios organized by feature (`aggregate`, `join`, `pivot`, `sort`, `where`, `xml`, `json`, `log`, and others).

## Test

Unit tests run under Node.js:

```bash
brew install node
node tests
```

To run a subset:

```bash
node tests [path1] [path2] ...
```

Each test compares its output against an expected result. To regenerate expected results after an intentional change:

```bash
node tests --rebase [path1] [path2] ...
```

## License

MIT. See [LICENSE](LICENSE).
