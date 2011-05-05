include ../include/dep_rule.mk

ASFLAGS = -f elf
CFLAGS+= -I"../include"
CXXFLAGS+= -I"../include"

dependencies = sprintf.d printf.d init.d debug.d iolib.d format.d dirlib.d m_malloc.d critical_section.d io.d signals.d bmalloc.d new.d

baseobjects = proc_entry.o start.o int.o reboot.o scheduler.o sprintf.o printf.o init.o debug.o iolib.o format.o dirlib.o structures parameters.o critical_section.o m_malloc.o pm_malloc.o bmalloc.o pbmalloc.o rawpiolib.o io.o signals.o psignals.o new.o

all : $(baseobjects) stdlibcpp.o stdlib.o malloc.o
	
pm_malloc.o : m_malloc.c
	$(CC) -c m_malloc.c $(CFLAGS) -o pm_malloc.o 

m_malloc.o : m_malloc.c
	$(CC) -c m_malloc.c $(CFLAGS) -o m_malloc.o -D SAFE

pbmalloc.o : bmalloc.c
	$(CC) -c bmalloc.c $(CFLAGS) -o pbmalloc.o 
	
bmalloc.o : bmalloc.c
	$(CC) -c bmalloc.c $(CFLAGS) -o bmalloc.o -D SAFE

# modify this to use a different malloc
# on services and stdlib
malloc.o : m_malloc.c
	cp m_malloc.o malloc.o

pmalloc.o : m_malloc.c
	cp pm_malloc.o pmalloc.o
	
.PHONY : structures
structures :
	$(MAKE) -C structures

int.o : int.s ../include/lib/int.h

reboot.o : reboot.s ../include/lib/reboot.h

scheduler.o : scheduler.s ../include/lib/scheduler.h

iolib.o : iolib.c ../include/lib/iolib.h
	$(CC) -c iolib.c $(CFLAGS) -o iolib.o -D IOLIB_MULTITHREADED

piolib.o : iolib.c ../include/lib/iolib.h
	$(CC) -c iolib.c $(CFLAGS) -o piolib.o -U IOLIB_MULTITHREADED
	
signals.o : signals.c ../include/lib/signals.h
	$(CC) -c signals.c $(CFLAGS) -o signals.o -D SIGNALS_MULTITHREADED

psignals.o : signals.c ../include/lib/signals.h
	$(CC) -c signals.c $(CFLAGS) -o psignals.o -U SIGNALS_MULTITHREADED

rawpiolib.o : rawiolib.c ../include/lib/iolib.h
	$(CC) -c rawiolib.c $(CFLAGS) -o rawpiolib.o -U IOLIB_MULTITHREADED

.PHONY : clean
clean :
	rm -f *.o *.d *~ *.map
	$(MAKE) -C structures clean


include ../include/dep_attach.mk

stdlib_libs = start.o proc_entry.o init.o ./../klib/kserv-user.o int.o scheduler.o sprintf.o printf.o piolib.o dirlib.o ./pmalloc.o ./structures/string.o ./structures/int64.o \
./structures/list.o ./structures/stdlibsim.o ./structures/int64.o debug.o ../drivers/screen/screen.o ./parameters.o

stdlib.o : $(stdlib_libs)
	$(LD) -i $(stdlib_libs) --Map stdlib.map  -o stdlib.o
	
stdlibcpp.o : $(stdlib_libs) new.o
	$(LD) -i $(stdlib_libs) new.o --Map stdlibcpp.map  -o stdlibcpp.o
