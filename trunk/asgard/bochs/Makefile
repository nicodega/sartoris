#all : 1.44a 10MB.hd.ofs
all : 1.44a

1.44a : FORCE
	#create sys image (modifying blocklist)
	
	./inject_size.sh "../bin/asgard.img" "1.44a" 

10MB.hd.ofs : FORCE
	# Create ofs hd disk partition with
	../tools/mkofs c 10MB.hdb.ofs 5242880 $$(./imgsize.pl ../bin/asgard.img)
	../tools/mkofs d 10MB.hdb.ofs /dev
	../tools/mkofs d 10MB.hdb.ofs /bin
	../tools/mkofs d 10MB.hdb.ofs /mnt
	../tools/mkofs dev 10MB.hdb.ofs /dev/tty0 devices/tty 0
	../tools/mkofs dev 10MB.hdb.ofs /dev/tty1 devices/tty 1
	../tools/mkofs dev 10MB.hdb.ofs /dev/tty2 devices/tty 2
	../tools/mkofs dev 10MB.hdb.ofs /dev/tty3 devices/tty 3
	../tools/mkofs dev 10MB.hdb.ofs /dev/tty4 devices/tty 4
	../tools/mkofs dev 10MB.hdb.ofs /dev/tty5 devices/tty 5
	../tools/mkofs dev 10MB.hdb.ofs /dev/tty6 devices/tty 6
	../tools/mkofs dev 10MB.hdb.ofs /dev/tty7 devices/tty 7
	../tools/mkofs dev 10MB.hdb.ofs /dev/hda devices/atac 32768
	../tools/mkofs dev 10MB.hdb.ofs /dev/hda0 devices/atac 0
	../tools/mkofs dev 10MB.hdb.ofs /dev/fdc devices/fdc 0
	../tools/mkofs a 10MB.hdb.ofs /bin/idle ../programs/idle/idle.raw
	../tools/mkofs a 10MB.hdb.ofs /bin/fdisk ../programs/fdisk/fdisk.raw
	../tools/mkofs a 10MB.hdb.ofs /bin/atactest ../programs/atactest/atactest.raw
	../tools/mkofs a 10MB.hdb.ofs /bin/jedi ../programs/jedi/jedi.raw
	../tools/mkofs a 10MB.hdb.ofs /bin/misbehav ../programs/misbehav/misbehav.raw

	#create boot sector (modify stage 1.5 possition on it and set partition table)
	dd if=../bin/asgard.img of=10MB.hdb.ofs.boot bs=35 count=1
	cat sectloc.hdb >> 10MB.hdb.ofs.boot
	dd if=../bin/asgard.img of=10MB.hdb.ofs.boot.1 bs=1 count=407 skip=39
	cat 10MB.hdb.ofs.boot.1 >> 10MB.hdb.ofs.boot
	rm 10MB.hdb.ofs.boot.1
	cat ptbl.hdb >> 10MB.hdb.ofs.boot
	
	#create sys image (modifying blocklist)
	
	./imgsize.pl ../bin/asgard.img -f

	#cut to the blocklist
	dd if=../bin/asgard.img of=10MB.hdb.ofs.sysimg bs=1 skip=512 count=904
	#insert the length on the blocklist and complete 3068 bytes
	cat blocklist.hdb >> 10MB.hdb.ofs.sysimg
	cat length-s.hdb >> 10MB.hdb.ofs.sysimg
	dd if=../bin/asgard.img of=10MB.hdb.ofs.sysimg.1 bs=1 skip=1422 count=2158	
	cat 10MB.hdb.ofs.sysimg.1 >> 10MB.hdb.ofs.sysimg
	#write the size 
	cat length.hdb >> 10MB.hdb.ofs.sysimg
	#get the rest of the asgard img from 3072
	dd if=../bin/asgard.img of=10MB.hdb.ofs.sysimg.1 bs=512 skip=6
	cat 10MB.hdb.ofs.sysimg.1 >> 10MB.hdb.ofs.sysimg
	rm 10MB.hdb.ofs.sysimg.1
	../tools/fill 10MB.hdb.ofs.sysimg -m 0x200
	
	#create ofs start (ofst)
	dd if=./10MB.hdb.ofs of=10MB.hdb.ofs.ofsstart bs=512 count=2
	
	#create remaining ofs disk
	dd if=./10MB.hdb.ofs of=10MB.hdb.ofs.ofsend bs=512 skip=$$(( ( ($$(./imgsize.pl ../bin/asgard.img) - 512) / 512) + 3 ))
	
	# create the disk
	cat 10MB.hdb.ofs > 10MB.hdb.ofs.raw
	rm 10MB.hdb.ofs
	cat 10MB.hdb.ofs.boot > 10MB.hdb.ofs
	cat 10MB.hdb.ofs.ofsstart >> 10MB.hdb.ofs
	cat 10MB.hdb.ofs.sysimg >> 10MB.hdb.ofs
	cat 10MB.hdb.ofs.ofsend >> 10MB.hdb.ofs
	
	#make the disk big enough for other 10MB partition
	../tools/fill 10MB.hdb.ofs -d 0xA00000
	
	#remove temp files
	rm 10MB.hdb.ofs.boot
	rm 10MB.hdb.ofs.ofsstart
	rm 10MB.hdb.ofs.ofsend
	rm 10MB.hdb.ofs.sysimg

	rm -f zeros
	rm length.hdb
	rm length-s.hdb

clean : FORCE
	rm -f *.ofs
	rm -f 1.44a
	rm -f 10MB.hd*
	rm -f zeros
	rm -f core
	rm -f *~
	rm -f *.out

clean_all: clean FORCE

FORCE :
