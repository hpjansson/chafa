#!/bin/sh

set -ev

cc $(pkg-config --libs --cflags chafa) example.c -o example
./example
