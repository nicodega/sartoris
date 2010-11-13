#all : 1.44a 2M.hd.ofs
all : 1.44a

1.44a : FORCE
	#create sys image (modifying blocklist)
	
	./imgsize.pl -f
	
	#cut to the blocklist
	dd if=../bin/asgard.img of=1.44a bs=1 count=1420
	
	#insert the length on the blocklist and complete 3068 bytes
	cat length-s.hdb >> 1.44a
	dd if=../bin/asgard.img of=1.44a.sysimg.1 bs=1 skip=1422 count=1646	
	cat 1.44a.sysimg.1 >> 1.44a
	
	#write the size
	cat length.hdb >> 1.44a
	
	#get the rest of the asgard img from 3072
	dd if=../bin/asgard.img of=1.44a.sysimg.1 bs=512 skip=6
	cat 1.44a.sysimg.1 >> 1.44a
	rm 1.44a.sysimg.1
	
	../tools/fill 1.44a -m 1474048	
	rm -f zeros
	rm length.hdb
	rm length-s.hdb

2M.hd.ofs : FORCE
	# Create ofs hd disk
	../tools/mkofs c 2M.hdb.ofs 2096640 $$(./imgsize.pl)
	../tools/mkofs d 2M.hdb.ofs /dev
	../tools/mkofs d 2M.hdb.ofs /bin
	../tools/mkofs d 2M.hdb.ofs /mnt
	../tools/mkofs dev 2M.hdb.ofs /dev/tty0 devices/tty 0
	../tools/mkofs dev 2M.hdb.ofs /dev/tty1 devices/tty 1
	../tools/mkofs dev 2M.hdb.ofs /dev/tty2 devices/tty 2
	../tools/mkofs dev 2M.hdb.ofs /dev/tty3 devices/tty 3
	../tools/mkofs dev 2M.hdb.ofs /dev/tty4 devices/tty 4
	../tools/mkofs dev 2M.hdb.ofs /dev/tty5 devices/tty 5
	../tools/mkofs dev 2M.hdb.ofs /dev/tty6 devices/tty 6
	../tools/mkofs dev 2M.hdb.ofs /dev/tty7 devices/tty 7
	../tools/mkofs dev 2M.hdb.ofs /dev/hda devices/atac 32768
	../tools/mkofs dev 2M.hdb.ofs /dev/hda0 devices/atac 0
	../tools/mkofs dev 2M.hdb.ofs /dev/fdc devices/fdc 0
	../tools/mkofs a 2M.hdb.ofs /bin/idle ../programs/idle/idle.raw
	../tools/mkofs a 2M.hdb.ofs /bin/fdisk ../programs/fdisk/fdisk.raw
	../tools/mkofs a 2M.hdb.ofs /bin/atactest ../programs/atactest/atactest.raw
	../tools/mkofs a 2M.hdb.ofs /bin/jedi ../programs/jedi/jedi.raw
	../tools/mkofs a 2M.hdb.ofs /bin/misbehav ../programs/misbehav/misbehav.raw
	
	#create boot sector (modify stage 1.5 possition on it)
	dd if=../bin/asgard.img of=2M.hdb.ofs.boot bs=35 count=1
	cat sectloc.hdb >> 2M.hdb.ofs.boot
	dd if=../bin/asgard.img of=2M.hdb.ofs.boot.1 bs=1 count=407 skip=39
	cat 2M.hdb.ofs.boot.1 >> 2M.hdb.ofs.boot
	rm 2M.hdb.ofs.boot.1
	cat ptbl.hdb >> 2M.hdb.ofs.boot
	
	#create sys image (modifying blocklist)
	
	./imgsize.pl -f
	#cut to the blocklist
	dd if=../bin/asgard.img of=2M.hdb.ofs.sysimg bs=1 skip=512 count=904
	#insert the blocklist and complete 3068 bytes
	cat blocklist.hdb >> 2M.hdb.ofs.sysimg
	cat length-s.hdb >> 2M.hdb.ofs.sysimg
	dd if=../bin/asgard.img of=2M.hdb.ofs.sysimg.1 bs=1 skip=1422 count=2158	
	cat 2M.hdb.ofs.sysimg.1 >> 2M.hdb.ofs.sysimg
	#write the size 
	cat length.hdb >> 2M.hdb.ofs.sysimg
	#get the rest of the asgard img from 3072
	dd if=../bin/asgard.img of=2M.hdb.ofs.sysimg.1 bs=512 skip=6
	cat 2M.hdb.ofs.sysimg.1 >> 2M.hdb.ofs.sysimg
	rm 2M.hdb.ofs.sysimg.1
	../tools/fill 2M.hdb.ofs.sysimg -m 0x200
	
	#create ofs start (boot sect and ofst)
	dd if=./2M.hdb.ofs of=2M.hdb.ofs.ofsstart bs=512 count=2
	
	#create remaining ofs disk
	dd if=./2M.hdb.ofs of=2M.hdb.ofs.ofsend bs=512 skip=$$(( ( ($$(./imgsize.pl) - 512) / 512) + 2 ))
	
	# create the disk
	cat 2M.hdb.ofs > 2M.hdb.ofs.raw
	rm 2M.hdb.ofs
	cat 2M.hdb.ofs.boot > 2M.hdb.ofs
	cat 2M.hdb.ofs.ofsstart >> 2M.hdb.ofs
	cat 2M.hdb.ofs.sysimg >> 2M.hdb.ofs
	cat 2M.hdb.ofs.ofsend >> 2M.hdb.ofs
	
	#make the disk big enough for other partition
	../tools/fill 2M.hdb.ofs -d 0x400000
	
	#remove temp files
	rm 2M.hdb.ofs.boot
	rm 2M.hdb.ofs.ofsstart
	rm 2M.hdb.ofs.ofsend
	rm 2M.hdb.ofs.sysimg

	rm -f zeros
	rm length.hdb
	rm length-s.hdb

clean : FORCE
	rm -f *.ofs
	rm -f 1.44a
	rm -f 2M.hd*
	rm -f zeros
	rm -f core
	rm -f *~
	rm -f *.out

clean_all: clean FORCE

FORCE :
