if [ ! -s kbin ]; then
    echo "there is no kbin! creating link to ../sartoris/arch/i386/sartoris"
    ln -s ../sartoris/arch/i386/sartoris kbin
fi

if [ ! -s klib ]; then
    echo "there is no klib! creating link to ../sartoris/arch/i386/sartoris"
    ln -s ../sartoris/arch/i386/sartoris klib
fi

if [ ! -s bin ]; then
    echo "there is no bin directory! creating one"
    mkdir bin
fi

cd include

if [ ! -s sartoris ]; then
    echo "there is no include/sartoris! creating link to ../sartoris/include/sartoris"
    ln -s ../../sartoris/include/sartoris sartoris
fi

if [ ! -s os ]; then
    echo "there is no include/os! creating link to include/asgard"
    ln -s asgard os
fi

if [ ! -s sartoris-target ]; then
    echo "there is no include/sartoris-target! creating link to ../sartoris/arch/target/include"
    ln -s ../../sartoris/arch/target/include sartoris-i386
fi

