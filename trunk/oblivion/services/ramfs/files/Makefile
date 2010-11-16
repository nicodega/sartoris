all : ramfs_files.img

ramfs_files.img : hello.raw mount.raw umount.raw ls.raw misbehav.raw jedi.raw idle.raw
	../../../tools/mkbfat ramfs_files.img

hello.raw : ../../../programs/helloworld/hello.raw
	cp ../../../programs/helloworld/hello.raw .

mount.raw : ../../../programs/mount/mount.raw
	cp ../../../programs/mount/mount.raw .

umount.raw : ../../../programs/umount/umount.raw
	cp ../../../programs/umount/umount.raw .

ls.raw : ../../../programs/ls/ls.raw
	cp ../../../programs/ls/ls.raw .

misbehav.raw : ../../../programs/misbehav/misbehav.raw
	cp ../../../programs/misbehav/misbehav.raw .

jedi.raw : ../../../programs/jedi/jedi.raw
	cp ../../../programs/jedi/jedi.raw .

idle.raw : ../../../programs/idle/idle.raw
	cp ../../../programs/idle/idle.raw .

.PHONY : clean
clean :
	rm -f zeros
	rm -f *.img
	rm -f *.raw
