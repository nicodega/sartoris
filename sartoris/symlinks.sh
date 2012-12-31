#!/bin/sh

cd include/sartoris

if [ ! -h target ]; then
    echo "there is no include arch-target! creating link to i386/include."
    ln -s ../../arch/i386/include target
fi

cd ../../arch

if [ ! -h target ]; then
    echo "there is no arch-target! creating link to i386."
    ln -s i386 target
fi

cd ..
