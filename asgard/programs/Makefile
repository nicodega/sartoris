CFLAGS += -I"../../include"
CXXFLAGS += -I"../../include"

subdirs = idle jedi misbehav sigtest atactest fdisk cpptest cat more stest fmaptst

export CC CFLAGS CFLAGS CXX CXXFLAGS LD AS ASFLAGS

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
	$(MAKE) -C cpptest clean
	$(MAKE) -C cat clean
	$(MAKE) -C more clean
	$(MAKE) -C stest clean
	$(MAKE) -C fmaptst clean
