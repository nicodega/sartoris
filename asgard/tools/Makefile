all : mkofs sld mkbinit2

CC = gcc

mkbinit2 : mkbinit2.c ../services/p-manager2/formats/initfs2.h
	$(CC) -I"../include" -o mkbinit2 mkbinit2.c

mkofs : mkofs.c 
	$(CC) -I"../include" -o mkofs mkofs.c
	
sld : sld.c 
	$(CC) -I"../include" -o sld sld.c

.PHONY : clean
clean : 
	rm -f *.o
	rm -f *~
	rm -f mkbinit mkofs 1.44a.ofs zeros
