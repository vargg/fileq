# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

`fileq` is a small C CLI (binary name `fq`) that keeps FIFO queues of text messages, one file per queue. Usage: `fq <command> [-q <queue_name> [<message>]]`. Commands: `help`, `list`, `show`, `size`, `clear`, `del`, `pull`, `push`.

## Build

```bash
make            # builds build/fq (gcc, -Wall, objects + .d header deps in build/)
make clean      # removes build/
make install    # copies build/fq to ~/.local/bin/fq
make test       # unit tests (ASan/UBSan build) + CLI checks
```

There is no linter.

## Tests

- **What `make test` runs:** `build/tests/test_args` and `build/tests/test_processing`, then `tests/cli.sh build/fq`.
- **One test at a time:** pass its name, e.g. `./build/tests/test_processing trim_keeps_order`. An unknown name exits 1.
- **The harness:** `tests/test.h` provides `TEST`/`RUN`/`CHECK*` and the helpers. Each unit test file `#include`s the `.c` it tests (never `main.c`), so `static` helpers like `_write_escaped` can be tested directly. New tests go in the matching file and need a `RUN(name)` line in its `main`.
- **Output and isolation:** handlers report through `printf`, so tests wrap calls in `capture_begin()`/`capture_end()`. Processing tests call `fresh_home()`, which points `HOME` at a new `mkdtemp` dir, so they never touch the real `~/.fileq`.
- **CLI checks:** `tests/cli.sh` covers what needs the real `main()`: exit codes, and the `help` version matching the README badge.
- **Concurrency:** `concurrent_push_pull` forks workers that push and pull on one queue. Removing the `flock` in `open_q` makes it fail. Each worker has a 30 s `alarm`, so a hang fails the test instead of blocking it.
- **Stale builds:** the macOS `make` only compares timestamps to the second. If you edit a source and rebuild within the same second, nothing gets rebuilt, so use `make -B` when that matters.

To try a change by hand, run the binary with a throwaway home, e.g. `HOME=$(mktemp -d) ./build/fq push -q test hi`.

## Architecture

- `src/main.c`: creates the storage dirs, parses args, and dispatches on the command with an if/else chain. The version lives here as `major`/`minor`/`patch` globals; when bumping it, also update the badge in `README.md`.
- `src/args.c` / `include/args.h`: argument parsing into `InputArgs`. **The order of `CommandTypes` matters.** `parse_args` groups commands by comparing against `LAST_NO_ARGS_COMMAND` (`LIST`) and `LAST_ONLY_Q_NAME_COMMAND` (`SIZE`): commands up to `LIST` take no args, commands up to `SIZE` take only `-q`, and anything after that also takes a message. A new command must go in the right group in the enum, the `commands[]` name table, `print_help`, and the dispatch in `main.c`. An unknown command is an error (exit 1), not a fallback to `help`. The queue name can be written `-q name`, `-q=name` or `-qname`, and `_is_valid_q_name` limits it to `[A-Za-z0-9._-]` with no leading `.`. That check is what stops names from escaping `~/.fileq/qs/`, and dot-names are reserved for `make_q`'s temporary files. All arguments after the queue name are joined with spaces to form the message.
- `src/processing.c` / `include/processing.h`: queue storage.

### Queue file format

Each queue is a file at `~/.fileq/qs/<queue_name>`:

- **Header:** three `u_int64_t` values (`FileMetadata`): `head` (byte offset of the oldest unread message), `skipped` (messages pulled but not yet removed from the file), and `size` (number of live messages).
- **Body:** one message per newline-terminated line. `_write_escaped` stores a newline inside a message as `\n` and a backslash as `\\`, and `_print_unescaped` decodes them on `pull`/`show`.

How each operation uses the file:

- `open_q` takes a `flock` that lasts until `destroy_q_obj` closes the file. Mode `"rb"` (`show`, `size`) gets a shared lock, and `"rb+"` (`push`, `pull`, `clear`) gets an exclusive one. Every read-modify-write of the header must happen inside an `open_q`/`destroy_q_obj` pair.
- `push` appends a line at EOF and increments `size`. It creates the queue first if needed: `make_q` writes the header to a temporary `.<name>.<pid>` file and `link()`s it into place, so a queue file never exists without a header. `list` skips dot-files.
- `pull` reads the line at `head` with `getline`, moves `head` forward, and increments `skipped`. It does not shrink the file. Once `skipped > max_num_of_skipped_items` (16), `_trim` shifts the remaining body back to just after the header and truncates the file. `_trim` copies raw bytes in `batch_size` (64) chunks.
- `clear` truncates the file to the header in place, under the lock. `del` unlinks it.