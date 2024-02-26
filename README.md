# ilish

A lisp to x86_64 compiler written purely in C.
Currently basing it on R7RS-small.

## Build

No external libraries are required. All you need is a C compiler and `make`. However, I work on this project with linux and gcc so other platforms and tools are untested.

Use `make` or `make release` to compile the compiler. You will find this binary in the `build/release` directory. 

There is also `make debug` which includes the flags for debugging and valgrind. This binary will be found in `build/debug`.

Additionally, use can use `make doc` to generate the basic documentation with `doxygen`. 

## Use

Currently single file compilation, REPL, and string evaluation are supported.

- To compile a file use the `-f` flag, i.e. `ilish -f filename.scm`. File extensions do not matter at the moment.
- To evaluate a string use the `-e` flag, i.e. `ilish -e "(+ 2 2)"`. Double quotes are not necessary but recommended to avoid any issues with your shell.
- REPL will be launched otherwise, i.e. `ilish`. Current REPL is quite minimal. I recommend using `rlwrap` to improve the experience.

Currently these will output x86_64 assembly.

I have also included a basic executable quickrun.sh which with `-f` or `-e` will compile the assembly with `cc` and run it. 
In current behaviour the compiler prints out the last expression.

## Language

Currently the supported symbols are:
- `+`, `-`, `*`, `/`, `modulo`, `1+`, `1-`.
- `=`, `>`, `>=`, `<`, `<=`, `and`, `or`, `zero?`, `one?`.
- `let`, `let*`, `lambda`, `set!`, and toplevel `define` for variables and functions.
- `if`, and `begin` for control.
- `cons`, `car`, `cdr`, `c[ad][ad]r`, `set-car!`, `set-cdr!`, `null?` and `pair?`.
- `make-vector`, `vector`, `vector?`,, `vector-ref`, `vector-set!`.
- `make-string`, `string`, `string?`, `string-length`, `string-ref`, and currently ascii-only `string-set!`.

Further implementation notes:
- GC works, but it is currently a WIP, and is not trustworthy at the moment.
- Vectors and Strings can be defined with #() and "" respectively.
- String operations are UTF-8 aware and are O(n) for it. However, pure ascii strings are tagged as such and will still be O(1).
- Objects and immediates are tagged for quick runtime checks and some optimizations are done to avoid them to begin with. Though, not all operations are safe, you can add two vector pointers for example.
- Lambdas support lexical scoping, tail-call optimizations, and boxing
