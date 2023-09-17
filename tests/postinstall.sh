#!/bin/sh

set -evx

cc -g \
    -fsanitize=address,undefined \
    -fsanitize-undefined-trap-on-error \
    $(pkg-config --libs --cflags chafa) \
    example.c \
    -o example

./example
