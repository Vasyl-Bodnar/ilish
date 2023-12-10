#!/bin/sh

if [[ "$2" = "" ]]; then
    ./build/debug/ilish.o -e "$1" > tmp.s
else 
    ./build/debug/ilish.o -f "$1" > tmp.s
fi

cat tmp.s

if [[ "$(head -n 1 tmp.s)" = ".global main" ]]; then
    gcc tmp.s -o tmp.o &>/dev/null
    ./tmp.o
    echo -e "Returned $?"
    rm tmp.o
fi

rm tmp.s
