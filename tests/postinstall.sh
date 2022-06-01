#!/bin/sh

set -evx

cc -g example.c $(pkg-config --libs --cflags chafa) -o example
./example
