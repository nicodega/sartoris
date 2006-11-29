all : init_images.img

testelf.img : ../../testelf/testelf.img
	cp ../../testelf/testelf.img .

init_images.img : atac.img console.img shell.img directory.img dma_man.img  fdc.img dma_man.img ofsservice.img pipes.img #testelf.img hdc.img
	rm -f init_images.img
	../../../tools/mkbinit init_images.img

atac.img : ../../atac/atac.img
	cp ../../atac/atac.img .

directory.img : ../../directory/directory.img
	cp ../../directory/directory.img .

dma_man.img : ../../dma_man/dma_man.img
	cp ../../dma_man/dma_man.img .

fdc.img : ../../fdc/fdc.img
	cp ../../fdc/fdc.img .

ofsservice.img : ../../ofsservice/ofsservice.img
	cp ../../ofsservice/ofsservice.img . 

console.img : ../../stdconsole/console.img
	cp ../../stdconsole/console.img . 

shell.img : ../../shell/shell.img
	cp ../../shell/shell.img .

hdc.img : ../../hdc/hdc.img
	cp ../../hdc/hdc.img .  

pipes.img : ../../pipes/pipes.img
	cp ../../pipes/pipes.img .  

.PHONY : clean
clean :
	rm -f zeros
	rm -f *.img
