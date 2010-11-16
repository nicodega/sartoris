CFLAGS = -O2 -ffreestanding -nostdinc -nostdlib -nostartfiles 
CXX = g++
CXXFLAGS = -O2 -ffreestanding -nostdinc -nostdlib -nostartfiles 
LD = ld
AS = nasm
ASFLAGS = -f elf


CC = /opt/crosstool/gcc-4.1.0-glibc-2.3.6/i686-unknown-linux-gnu/bin/i686-unknown-linux-gnu-gcc
CXX = /opt/crosstool/gcc-4.1.0-glibc-2.3.6/i686-unknown-linux-gnu/bin/i686-unknown-linux-gnu-g++
LD = /opt/crosstool/gcc-4.1.0-glibc-2.3.6/i686-unknown-linux-gnu/bin/i686-unknown-linux-gnu-ld


export CC CFLAGS CXX CXXFLAGS LD AS ASFLAGS

.SUFIXXES: # delete the default suffixes

%.o : %.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) -s $< -o $@

%.o : %.s
	$(AS) $(ASFLAGS) $<

%.o : %.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) -s $< -o $@ 

subdirs = tools lib drivers services init programs 

.PHONY : oblivion
oblivion : $(subdirs) bochs

.PHONY : bochs
bochs :
	cp kbin/sartoris.img bin/oblivion.img
	cat init/init.img >> bin/oblivion.img
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
	$(MAKE) -C init  clean
	$(MAKE) -C bochs clean
	$(MAKE) -C lib clean
	$(MAKE) -C tools clean
	rm -f bin/oblivion.img

clean_all : clean 

.PHONY : tarball
tarball :
	cd .. && tar cvf oblivion.tar oblivion && gzip oblivion.tar

.PHONY : install_disk
install_disk :
	dd if=bin/oblivion.img of=disk

.PHONY : install_floppy
install_floppy :
	dd if=bin/oblivion.img of=floppy
