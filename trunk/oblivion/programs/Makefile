CPPFLAGS += -I"../../include"

subdirs = idle helloworld misbehav jedi mmx prmtst dbg

export CC CFLAGS CPPFLAGS CXX CXXFLAGS LD AS ASFLAGS

.PHONY : all
all : $(subdirs)

.PHONY : $(subdirs)
$(subdirs) :
	$(MAKE) -C $@

.PHONY : clean
clean : 
	$(MAKE) -C idle clean
	$(MAKE) -C helloworld clean
	$(MAKE) -C misbehav clean
	$(MAKE) -C c++test clean
	$(MAKE) -C jedi clean
	$(MAKE) -C mmx clean
	$(MAKE) -C dbg clean
