#!/bin/sh

if [[ "$2" = "" ]]; then
    ./build/debug/ilish -e "$1" > tmp.s
else 
    ./build/debug/ilish -f "$1" > tmp.s
fi

cat tmp.s

if [[ "$(head -n 1 tmp.s)" = ".data" ]]; then
    gcc -pg -Og -ggdb3 tmp.s runtime/runtime.c -o tmp # using cc to call gas and ld
    # as tmp.s -o tmp.o # using gas and ld directly
    # ld tmp.o build/runtime/runtime.o -o tmp -lc
    ./tmp
    rm tmp.o tmp &>/dev/null
fi

rm tmp.s
