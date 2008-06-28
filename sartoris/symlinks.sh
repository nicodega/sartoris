#!/bin/sh

cd arch

if [ ! -h target ]; then
    echo "there is no arch-target! creating link to i386."
    ln -s i386 target
fi

cd ..
