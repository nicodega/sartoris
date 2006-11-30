%.d : %.c
	@echo making deps for $< ...
	@$(CC) -E -MD $(CFLAGS) $< > /dev/null

%.d : %.cpp
	@echo making deps for $< ...
	@$(CC) -E -MD $(CXXFLAGS) $< > /dev/null