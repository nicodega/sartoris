include ../../include/dep_rule.mk

CFLAGS += -I"../../include"

objects = atac.o ata_find.o atac_stddev.o ataioint.o ataioisa.o ioctrl.o ataioreg.o ataiosub.o blockdev.o channelwp.o command_queue.o pio.o tmr.o logic_devices.o helpers.o $(extobjects)

extobjects = ../../lib/structures/stdlibsim.o ../../lib/structures/lptree.o ../../klib/kserv.o ../../lib/signals.o ../../lib/io.o ../../lib/structures/string.o ../../lib/structures/int64.o ../../lib/m_malloc.o ../../lib/structures/list.o ../../lib/critical_section.o ../../lib/int.o ../../lib/scheduler.o ../../lib/sprintf.o ../../drivers/screen/screen.o ../../lib/debug.o

dependencies = ata_find.d atac.d atac_stddev.d ataioint.d ioctrl.d ataioisa.d ataioreg.d ataiosub.d blockdev.d channelwp.d command_queue.d pio.d tmr.d logic_devices.d helpers.d

all : atac.img 

atac.img : $(objects)
	$(LD) -T ../../tools/linkscript --Map atac.map -o atac.elf.img $(objects)
	../../tools/sld atac.elf.img atac.img
	#rm atac.elf.img
	
.PHONY : clean
clean :
	rm -f *~ *.o *.d *.img *.map zeros

include ../../include/dep_attach.mk

