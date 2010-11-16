all : 1.44a

1.44a : FORCE	
	#create sys image (modifying blocklist)
	
	./imgsize.pl -f
	
	#cut to the blocklist
	dd if=../bin/oblivion.img of=1.44a bs=1 count=1420
	
	#insert the length on the blocklist and complete 3068 bytes
	cat length-s.hdb >> 1.44a
	dd if=../bin/oblivion.img of=1.44a.sysimg.1 bs=1 skip=1422 count=1646	
	cat 1.44a.sysimg.1 >> 1.44a
	
	#write the size
	cat length.hdb >> 1.44a
	
	#get the rest of the oblivion img from 3072
	dd if=../bin/oblivion.img of=1.44a.sysimg.1 bs=512 skip=6
	cat 1.44a.sysimg.1 >> 1.44a
	rm 1.44a.sysimg.1
	
	../tools/fill 1.44a -m 1474048	
	rm -f zeros
	rm length.hdb
	rm length-s.hdb

clean : FORCE
	rm -f 1.44a
	rm -f zeros
	rm -f core
	rm -f *~
	rm -f *.out

clean_all: clean FORCE

FORCE :
