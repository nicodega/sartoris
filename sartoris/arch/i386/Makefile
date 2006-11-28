export CC CFLAGS LD AS ASFLAGS

subdirs = kernel boot lib sartoris

.PHONY : all
all : $(subdirs)

.PHONY : $(subdirs)
$(subdirs) :
	$(MAKE) -C $@

# dependencies

boot : kernel
sartoris : boot kernel lib

.PHONY : clean
clean :
	$(MAKE) -C boot clean
	$(MAKE) -C kernel clean
	$(MAKE) -C lib clean
	$(MAKE) -C sartoris clean
	rm -f include/*~

clean_all : clean

.PHONY : dep
dep :
	$(MAKE) -C boot dep
	$(MAKE) -C kernel dep
	$(MAKE) -C lib dep
	$(MAKE) -C sartoris dep
