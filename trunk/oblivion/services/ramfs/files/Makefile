all : ramfs_files.img

ramfs_files.img : hello.raw misbehav.raw jedi.raw idle.raw mmx.raw prmtst.raw
	../../../tools/mkbfat ramfs_files.img

hello.raw : ../../../programs/helloworld/hello.raw
	cp ../../../programs/helloworld/hello.raw .

misbehav.raw : ../../../programs/misbehav/misbehav.raw
	cp ../../../programs/misbehav/misbehav.raw .

jedi.raw : ../../../programs/jedi/jedi.raw
	cp ../../../programs/jedi/jedi.raw .

idle.raw : ../../../programs/idle/idle.raw
	cp ../../../programs/idle/idle.raw .
	
mmx.raw : ../../../programs/mmx/mmx.raw
	cp ../../../programs/mmx/mmx.raw .
	
prmtst.raw : ../../../programs/prmtst/prmtst.raw
	cp ../../../programs/prmtst/prmtst.raw .

.PHONY : clean
clean :
	rm -f zeros
	rm -f *.img
	rm -f *.raw
