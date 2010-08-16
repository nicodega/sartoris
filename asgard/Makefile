CC = /opt/crosstool/gcc-4.1.0-glibc-2.3.6/i686-unknown-linux-gnu/bin/i686-unknown-linux-gnu-gcc
CXX = /opt/crosstool/gcc-4.1.0-glibc-2.3.6/i686-unknown-linux-gnu/bin/i686-unknown-linux-gnu-g++
LD = /opt/crosstool/gcc-4.1.0-glibc-2.3.6/i686-unknown-linux-gnu/bin/i686-unknown-linux-gnu-ld

CFLAGS = -O2 -ffreestanding -nostdinc -nostdlib -nostartfiles 
CXXFLAGS = -O2 -nostdinc -nostdlib -nostartfiles -fno-rtti -fno-exceptions -D CPLUSPLUS
AS = nasm
ASFLAGS = -f elf

export CC CFLAGS CXX CXXFLAGS LD AS ASFLAGS

.SUFIXXES: # delete the default suffixes

%.o : %.cpp
    $(CXX) -c $(CXXFLAGS) -s $< -o $@ 

%.o : %.c
    $(CC) -c $(CFLAGS) -s $< -o $@

%.o : %.s
    $(AS) $(ASFLAGS) $<


subdirs = tools lib drivers services programs doc 

.PHONY : oblivion
oblivion : $(subdirs) bochs

.PHONY : bochs
bochs :
    cp kbin/sartoris.img bin/asgard.img
    cat services/p-manager2/pmanager2.img >> bin/asgard.img
    $(MAKE) -C bochs

.PHONY : $(subdirs)
$(subdirs) :
    $(MAKE) -C $@

#dependencies

drivers : lib

services : lib drivers programs tools

init : lib services

programs : lib

.PHONY : clean
clean : 
    $(MAKE) -C programs clean
    $(MAKE) -C drivers clean
    $(MAKE) -C services clean
    $(MAKE) -C bochs clean
    $(MAKE) -C lib clean
    $(MAKE) -C tools clean
    rm -f bin/*.img

.PHONY : dep
dep : 
    $(MAKE) -C programs dep
    $(MAKE) -C drivers dep
    $(MAKE) -C services dep
    $(MAKE) -C bochs dep
    $(MAKE) -C lib dep
    $(MAKE) -C tools dep

clean_all : clean 

.PHONY : tarball
tarball : clean
    cd .. && tar cvf asgard.tar asgard && gzip asgard.tar

.PHONY : install_disk
install_disk :
    dd if=bin/asgard.img of=disk

.PHONY : install_floppy
install_floppy :
    dd if=bin/asgard.img of=floppy

