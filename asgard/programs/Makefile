CPPFLAGS += -I"../../include"

subdirs = idle jedi misbehav sigtest atactest fdisk

export CC CFLAGS CPPFLAGS CXX CXXFLAGS LD AS ASFLAGS

.PHONY : all
all : $(subdirs)

.PHONY : $(subdirs)
$(subdirs) :
	$(MAKE) -C $@

.PHONY : clean
clean : 
	$(MAKE) -C idle clean
	$(MAKE) -C misbehav clean
	$(MAKE) -C jedi clean
	$(MAKE) -C ramdisk clean
	$(MAKE) -C sigtest clean
	$(MAKE) -C atactest clean
	$(MAKE) -C fdisk clean
