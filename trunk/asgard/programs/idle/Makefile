.PHONY : all
all : 
	nasm -f elf idle.asm -o idle.o
	$(LD) -T ../../tools/linkscript --Map idle.map -o idle.elf.raw idle.o
	../../tools/sld idle.elf.raw idle.raw
	#rm idle.elf.raw
	
.PHONY : clean
clean : 
	rm -f *~ *.o *.raw *.map zeros

