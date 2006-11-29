include ../../include/dep_rule.mk

CPPFLAGS += -I"../../include"

objects = ofsservice.o bitmaparray.o bitmaps.o cached_devices.o changeatt.o close.o create_file.o chardevrw.o \
		delete.o device_locking.o devices.o dir_parser.o filebuffers-shared.o fileinfo.o flush.o \
		helpers.o initofs.o linkfile.o main_helpers.o mkdevice.o mkdir.o mount.o node_locking.o \
		nodes.o open.o read.o seek.o shutdown_helpers.o umount.o working_process.o path_cache.o devblocks_cache.o \
		wp_signaling.o write.o exists.o ioctl.o tell.o ../../drivers/screen/screen.o ../../lib/debug.o $(extobjects)

extobjects = ../../lib/structures/stdlibsim.o ../../lib/structures/lptree.o ../../lib/structures/string.o ../../klib/kserv.o ../../lib/structures/avltree.o ../../lib/structures/int64.o ../../lib/m_malloc.o ../../lib/structures/list.o ../../lib/critical_section.o ../../lib/int.o ../../lib/scheduler.o

dependencies = ofsservice.d bitmaparray.d bitmaps.d cached_devices.d changeatt.d close.d create_file.d \
		delete.d device_locking.d devices.d dir_parser.d filebuffers-shared.d fileinfo.d flush.d \
		helpers.d initofs.d linkfile.d main_helpers.d mkdevice.d mkdir.d mount.d node_locking.d \
		nodes.d open.d read.d seek.d shutdown_helpers.d umount.d working_process.d \
		wp_signaling.d write.d exists.d ioctl.d tell.d chardevrw.d filebuffers-shared.d path_cache.d devblocks_cache.d

all : ofsservice.img 

ofsservice.img : $(objects)
	$(LD) -T ../../tools/linkscript --Map ofsservice.map -o ofsservice.elf.img $(objects)
	../../tools/sld ofsservice.elf.img ofsservice.img
	#rm ofsservice.elf.img
	
.PHONY : clean
clean :
	rm -f *~ *.o *.d *.img *.map zeros

include ../../include/dep_attach.mk

