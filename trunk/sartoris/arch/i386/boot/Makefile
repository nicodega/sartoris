all : bootsect

bootsect : stage1.s stage1_5.s stage2mem.s sartoris_loader.s ../kernel/kimg
	nasm -f bin stage1.s -o stage1 -l stage1.map
	nasm -f bin stage1_5.s -o stage1_5 -l stage1_5.map
	nasm -f bin stage2mem.s -o stage2 -l stage2.map
	nasm -f bin -o sartoris_loader -dkern_sectors=`./kernelblocks.pl` -l sartoris_loader.map sartoris_loader.s 
	rm -f bootsect
	cat stage1 >> bootsect
	cat stage1_5 >> bootsect
	cat stage2 >> bootsect
	cat sartoris_loader >> bootsect

.PHONY : clean
clean :
	rm -f bootsect boot.map
	rm -f *~

clean_all : clean

dep : 
