all : mkbfat

mkbfat : mkbfat.c ../include/lib/bfilesys.h
	gcc -I"../include" -o mkbfat mkbfat.c

.PHONY : clean
clean : 
	rm -f *.o mkbfat
	rm -f *~
	rm -f mkbfat
