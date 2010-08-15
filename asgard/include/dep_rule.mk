%.d : %.c
	@echo making deps for $< ...
	@$(CC) -E -MD $(CFLAGS) $< > /dev/null

%.d : %.cpp
	@echo making deps for $< ...
	@$(CXX) -E -MD $(CXXFLAGS) $< > /dev/null