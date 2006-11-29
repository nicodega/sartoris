.PHONY : dep
dep : $(dependencies)

ifeq ($(filter clean clean_all dep,$(MAKECMDGOALS)), )
include $(dependencies)
endif