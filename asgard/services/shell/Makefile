include ../../include/dep_rule.mk

CFLAGS += -I"../../include" 

objects = shell_entry.o shell.o initialization.o terminal.o run_internal.o run.o cd.o env.o ls.o mount.o io.o set.o contest.o jobs.o process.o execute.o atactst.o dyntst.o $(extobjects)

extobjects = ../../klib/kserv.o ../../lib/scheduler.o ../../lib/sprintf.o ../../lib/int.o ../../lib/debug.o ../../drivers/screen/screen.o ../../lib/malloc.o ../../lib/critical_section.o \
../../lib/structures/string.o ../../lib/structures/int64.o ../../lib/piolib.o ../../lib/structures/lptree.o ../../lib/structures/list.o ../../lib/structures/stdlibsim.o ../../lib/format.o ../../lib/parameters.o

dependencies = shell.d cd.d env.d ls.d mount.d io.d set.d initialization.d terminal.d run_internal.d run.d process.d execute.d jobs.d atactst.d dyntst.d

all : shell.img 

.PHONY : shell.img
shell.img : $(objects) 
	$(LD) -T ../../tools/linkscript --Map shell.map -o shell.elf.img $(objects)
	../../tools/sld shell.elf.img shell.img
	#rm shell.elf.img
	
.PHONY : clean
clean :
	rm -f *~ *.o *.img *.d *.map zeros

include ../../include/dep_attach.mk

