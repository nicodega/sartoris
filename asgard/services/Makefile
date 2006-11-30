subdirs = stdconsole fdc dma_man shell directory ofsservice pipes pci atac p-manager2 

all : services

.PHONY : services
services : $(subdirs)

.PHONY : $(subdirs)
$(subdirs) :
	$(MAKE) -C $@

.PHONY : clean
clean :
	$(MAKE) -C p-manager2 clean
	$(MAKE) -C fdc clean
	$(MAKE) -C dma_man clean
	$(MAKE) -C shell clean
	$(MAKE) -C directory clean
	$(MAKE) -C ofsservice clean
	$(MAKE) -C stdconsole clean
	$(MAKE) -C pipes clean
	$(MAKE) -C pci clean
	$(MAKE) -C atac clean

.PHONY : dep
dep :
	$(MAKE) -C p-manager2 dep
	$(MAKE) -C fdc dep
	$(MAKE) -C dma_man dep
	$(MAKE) -C shell dep
	$(MAKE) -C directory dep
	$(MAKE) -C ofsservice dep
	$(MAKE) -C stdconsole dep
	$(MAKE) -C pipes dep
	$(MAKE) -C pci dep
	$(MAKE) -C atac dep
