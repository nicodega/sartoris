all : kserv.o kserv-user.o

kserv.o : kserv.s
	nasm -f elf kserv.s -o kserv.o


kserv-user.o : kserv-user.s
	nasm -f elf kserv-user.s -o kserv-user.o

.PHONY : clean
clean : 
	rm -f *.o
	rm -f *~
	rm -f ../../../lib/kserv.o

dep :
