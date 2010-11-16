subdirs = p-manager console filesys fdc ramfs dma_man

all : services

.PHONY : services
services : $(subdirs)

.PHONY : $(subdirs)
$(subdirs) :
	$(MAKE) -C $@

.PHONY : clean
clean :
	$(MAKE) -C  p-manager clean
	$(MAKE) -C  console clean
	$(MAKE) -C filesys clean
	$(MAKE) -C fdc clean
	$(MAKE) -C ramfs clean
	$(MAKE) -C dma_man clean
