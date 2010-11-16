CPPFLAGS += -I"../../include"

CFLAGS += -Os

objects = ramfs.o $(extobjects)
extobjects = ../../klib/kserv-user.o ../../lib/bfilesys.o

all : ramfs.img

.PHONY : ramfs.img
ramfs.img : ramfs_code.img
	$(MAKE) -C files all
	cp ramfs_code.img ramfs.img
	cat files/ramfs_files.img >> ramfs.img
	./fill.pl

ramfs_code.img : ramfs.o
	$(LD) --oformat binary --entry 0x00000000 --Ttext 0x00000000 -N \
--Map ramfs.map -o ramfs_code.img $(objects)
	./fill_code.pl

# dependencies

ramfs.o : ramfs.c ramfs_internals.h ../../include/services/ramfs/ramfs.h

.PHONY : clean
clean :
	$(MAKE) -C files clean
	rm -f *.img
	rm -f *.o
	rm -f *~
	rm -f *.map zeros
