# ilish

A lisp to x86_64 compiler written purely in C.
This lisp is not following any specific standards. However, it is loosely based on Scheme.

## Build

No external libraries are required. All you need is a C compiler and a `make` tool. However, I work on this project with linux and gcc so other platforms are untested.

Use `make` or `make release` to compile the compiler. You will find this binary in the `build/release` directory. 

There is also `make debug` which includes the flags for debugging and valgrind. This binary will be found in `build/debug`.

Additionally, use can use `make doc` to generate the documentation with `doxygen`. 

## Use

Currently single file compilation, REPL, and string evaluation are supported.

- To compile a file use the `-f` flag, i.e. `ilish -f filename.il`. File extensions do not matter. I use .il as I did not find any significant extension conflicts for that.
- To evaluate a string use the `-e` flag, i.e. `ilish -e "(+ 2 2)"`. Double quotes are not necessary but recommended to avoid any issues with your shell.
- REPL will be launched otherwise, i.e. `ilish`. Current REPL is quite minimal. I recommend using `rlwrap` to improve the experience.

Currently these will output x86_64 assembly.

## Language

Supported symbols are `+`, `-`, `*`, `/`, `1+`, `1-`, `let`, `let*`, `if`.

Use any of `[]`, `()`, `{}`. They are completely equivalent.

Simple example `(let ((a 2)) (+ 1 a))`
