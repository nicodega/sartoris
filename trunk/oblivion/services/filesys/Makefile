CPPFLAGS += -I"../../include/"

objects = filesys.o $(extobjects)
extobjects = ../../klib/kserv.o ../../drivers/screen/screen.o

all : filesys.img  

.PHONY : fdimage
fdimage : floppy.img
	dd if=fd0.img of=/dev/fd0 bs=512 count=17

.PHONY : floppy.img
floppy.img :
	$(MAKE) -C files all
	cp mbr.raw fd0.img
	cat files/file0.img >> fd0.img
	cat files/file1.img >> fd0.img
	./fill.pl

filesys.img : $(objects) 
	$(LD) --oformat binary --entry 0x00000000 --Ttext 0x00000000 -N \
--Map filesys.map -o filesys.img $(objects)
	./fill_filesys.pl

.PHONY : clean
clean :
	rm -f *~
	rm -f *.o
	rm -f filesys.img fd0.img zeros filesys.map
