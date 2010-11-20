all : int.o reboot.o scheduler.o malloc.o sprintf.o printf.o init.o bfilesys.o print.o

CPPFLAGS+= -I"../include"

int.o : int.s ../include/lib/int.h
	nasm -f elf int.s -o int.o

reboot.o : reboot.s ../include/lib/reboot.h
	nasm -f elf reboot.s -o reboot.o

scheduler.o : scheduler.s ../include/lib/scheduler.h
	nasm -f elf scheduler.s -o scheduler.o

malloc.o : malloc.c ../include/lib/malloc.h

sprintf.o : sprintf.c ../include/lib/printf.h

printf.o : printf.c ../include/lib/printf.h

init.o : init.c

print.o : print.c

bfilesys.o : bfilesys.c ../include/lib/bfilesys.h

clean : FORCE
	rm -f *.o
	rm -f *~

FORCE :
