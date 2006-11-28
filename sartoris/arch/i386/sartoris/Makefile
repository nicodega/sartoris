.PHONY : all
all :
	cp ../boot/bootsect sartoris.img
	cat ../kernel/kimg >> sartoris.img 	
	cp ../lib/kserv.o ../lib/kserv-user.o .



.PHONY : clean
clean :
	rm -f *.img *.o
.PHONY : dep
dep :
