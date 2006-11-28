include arch/target/compenv.mk

CPPFLAGS += -D__KERNEL__

export CC CFLAGS CPPFLAGS LD AS ASFLAGS

subdirs = mk arch/target

.PHONY : sartoris
sartoris : $(subdirs)

.PHONY : satoris-emu
sartoris-emu :
	$(MAKE) -C mk clean 
	cd arch; rm -f target; ln -s Li386emu target; cd ..
	$(MAKE) sartoris

.PHONY : sartoris-i386
sartoris-i386 :
	$(MAKE) -C mk clean
	cd arch; rm -f target; ln -s i386 target; cd ..
	$(MAKE) sartoris


.PHONY : $(subdirs)
$(subdirs) :
	$(MAKE) -C $@

.PHONY : dep
dep : 
	$(MAKE) -C mk dep
	$(MAKE) -C arch/target dep

arch/target : target mk

.PHONY : target
target :
	./symlinks.sh

.PHONY : doc
doc :
	$(MAKE) -C doc

.PHONY : clean
clean : target 
	rm -f *~
	$(MAKE) -C arch/target clean
	$(MAKE) -C mk clean

.PHONY : clean_all
clean_all : 
	rm -f *~
	$(MAKE) -C arch/i386 clean_all
	$(MAKE) -C arch/Li386emu clean_all
	$(MAKE) -C mk clean_all
	$(MAKE) -C doc clean_all
	rm -rf *~ \#*

.PHONY : tarball
tarball : 
	cd .. && rm -f sartoris.tar sartoris.tar.gz && tar cvf sartoris.tar sartoris && gzip sartoris.tar
