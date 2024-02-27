#!/bin/sh

case "$1" in
    "-f") ./build/debug/ilish -f "$2" > tmp.s;;
    "-fs") shift; ./build/debug/ilish -fs "$@" > tmp.s;;
    "-fd") ./build/debug/ilish -f "$2" > tmp.s;;
    "-e") ./build/debug/ilish -e "$2" > tmp.s;;
    "-ed") ./build/debug/ilish -e "$2" > tmp.s;;
esac

cat tmp.s

gcc -pg -Og -ggdb3 tmp.s runtime/runtime.c -o tmp

./tmp

case "$1" in
    "-fd") gdb tmp;;
    "-ed") gdb tmp;;
    *) rm tmp tmp.s;;
esac
