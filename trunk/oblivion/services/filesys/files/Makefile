all : file_images.img

file_images.img : FORCE
	cp ../../../programs/helloworld/hello.raw file0.raw
	cp ../../../programs/idle/idle.raw file1.raw
	cp file0.raw file0.img
	cp file1.raw file1.img
	cp file2.raw file2.img
	cp file3.raw file3.img
	cp file4.raw file4.img
	cp file5.raw file5.img
	cp file6.raw file6.img
	
	./fill0.pl
	./fill1.pl
	./fill2.pl
	./fill3.pl
	./fill4.pl
	./fill5.pl
	./fill6.pl
	cp file0.img ramfs_files.img
	cat file1.img >> ramfs_files.img
	cat file2.img >> ramfs_files.img
	cat file3.img >> ramfs_files.img
	cat file4.img >> ramfs_files.img
	cat file5.img >> ramfs_files.img
	cat file6.img >> ramfs_files.img

clean : FORCE
	rm -f zeros
	rm -f *.img

FORCE :
