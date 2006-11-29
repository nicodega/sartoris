#CC = gcc
CFLAGS = -O2 -ffreestanding -nostdinc -nostdlib -nostartfiles 
CXX = g++
CXXFLAGS = -O2 -ffreestanding -nostdinc -nostdlib -nostartfiles 
#LD = ld
AS = nasm
ASFLAGS = -f elf


export CC CFLAGS CXX CXXFLAGS LD AS ASFLAGS

.SUFIXXES: # delete the default suffixes



%.o : %.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) -s $< -o $@

%.o : %.s
	$(AS) $(ASFLAGS) $<

%.o : %.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) -s $< -o $@ 

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

