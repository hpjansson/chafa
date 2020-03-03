#!/bin/sh

set -evx

cc example.c $(pkg-config --libs --cflags chafa) -o example
./example
