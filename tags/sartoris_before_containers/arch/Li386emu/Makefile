subdirs = emulator kernel lib sartoris

.PHONY : all
all : $(subdirs)

.PHONY : $(subdirs)
$(subdirs) :
	$(MAKE) -C $@

# dependencies

sartoris : emulator kernel lib

.PHONY : clean
clean :
	$(MAKE) -C emulator clean
	$(MAKE) -C kernel clean
	$(MAKE) -C lib clean
	$(MAKE) -C sartoris clean

clean_all : clean

.PHONY : dep
dep :
	$(MAKE) -C emulator dep
	$(MAKE) -C kernel dep
	$(MAKE) -C lib dep
	$(MAKE) -C sartoris dep
