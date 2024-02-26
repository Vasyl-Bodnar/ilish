#!/bin/sh

if [[ "$1" = "-f" ]]; then
    ./build/debug/ilish -f "$2" > tmp.s
else
    ./build/debug/ilish -e "$2" > tmp.s
fi

cat tmp.s

if [[ "$(head -n 1 tmp.s)" = ".data" ]]; then
    gcc -pg -Og -ggdb3 tmp.s runtime/runtime.c -o tmp 
    # as tmp.s -o tmp.o # using gas and ld directly
    # ld tmp.o build/runtime/runtime.o -o tmp -lc
    ./tmp
    if [[ "$1" != "-t" ]]; then
        rm tmp &>/dev/null
    fi
fi

if [[ "$1" != "-t" ]]; then
    rm tmp.s
fi
