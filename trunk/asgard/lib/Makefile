include ../include/dep_rule.mk

ASFLAGS = -f elf

dependencies = sprintf.d printf.d init.d debug.d iolib.d format.d dirlib.d m_malloc.d critical_section.d io.d signals.d 

all : proc_entry.o start.o int.o reboot.o scheduler.o sprintf.o printf.o init.o debug.o iolib.o format.o dirlib.o structures stdlib.o parameters.o critical_section.o m_malloc.o pm_malloc.o rawpiolib.o io.o signals.o psignals.o

pm_malloc.o : m_malloc.c
	$(CC) -c m_malloc.c $(CPPFLAGS) -o pm_malloc.o 

m_malloc.o : m_malloc.c
	$(CC) -c m_malloc.c $(CPPFLAGS) -o m_malloc.o -D SAFE

.PHONY : structures
structures :
	$(MAKE) -C structures

CPPFLAGS+= -I"../include"

int.o : int.s ../include/lib/int.h

reboot.o : reboot.s ../include/lib/reboot.h

scheduler.o : scheduler.s ../include/lib/scheduler.h

iolib.o : iolib.c ../include/lib/iolib.h
	$(CC) -c iolib.c $(CPPFLAGS) -o iolib.o -D IOLIB_MULTITHREADED

piolib.o : iolib.c ../include/lib/iolib.h
	$(CC) -c iolib.c $(CPPFLAGS) -o piolib.o -U IOLIB_MULTITHREADED
	
signals.o : signals.c ../include/lib/signals.h
	$(CC) -c signals.c $(CPPFLAGS) -o signals.o -D SIGNALS_MULTITHREADED

psignals.o : signals.c ../include/lib/signals.h
	$(CC) -c signals.c $(CPPFLAGS) -o psignals.o -U SIGNALS_MULTITHREADED

rawpiolib.o : rawiolib.c ../include/lib/iolib.h
	$(CC) -c rawiolib.c $(CPPFLAGS) -o rawpiolib.o -U IOLIB_MULTITHREADED

.PHONY : clean
clean :
	rm -f *.o *.d *~ *.map
	$(MAKE) -C structures clean


include ../include/dep_attach.mk

stdlib_libs = start.o proc_entry.o init.o ./../klib/kserv-user.o int.o scheduler.o sprintf.o printf.o piolib.o dirlib.o ./pm_malloc.o ./structures/string.o ./structures/list.o ./structures/stdlibsim.o ./structures/int64.o debug.o ../drivers/screen/screen.o ./parameters.o

stdlib.o : $(stdlib_libs)
	$(LD) -i $(stdlib_libs) --Map stdlib.map  -o stdlib.o

