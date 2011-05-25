
export CC CFLAGS CXX CXXFLAGS LD AS ASFLAGS

.PHONY : all
all : 
	$(MAKE) -C screen all
	$(MAKE) -C keyboard all
	$(MAKE) -C pit all
	$(MAKE) -C dma all
	$(MAKE) -C rtc all
	$(MAKE) -C floppy all
	$(MAKE) -C mouse all

.PHONY : clean
clean : 
	$(MAKE) -C screen clean
	$(MAKE) -C keyboard clean
	$(MAKE) -C pit clean
	$(MAKE) -C dma clean
	$(MAKE) -C rtc clean
	$(MAKE) -C floppy clean
	$(MAKE) -C mouse clean