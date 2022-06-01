#!/bin/sh

set -evx

cc -g -fsanitize=address example.c $(pkg-config --libs --cflags chafa) -o example
./example
