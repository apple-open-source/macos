NAME = numpy
VERSION = 1.2.1

include ../Makefile.inc

HERE = $(shell pwd)

custompatching:
	sed -e 's,===CONFIGH===,$(HERE)/config.h.ed,' -e 's,===NUMPYCONFIG===,$(HERE)/numpyconfig.h.ed,' numpy_core_setup.py.ed | \
	    ed - $(DIR)/numpy/core/setup.py
	ed - $(DIR)/numpy/core/src/scalarmathmodule.c.src < numpy_core_src_scalarmathmodule.c.src.ed

postinstall:
	cp -f $(DIR)/LICENSE.txt $(OSL)/$(NAME)-$(VERSION).txt
	cp -f $(NAME).partial $(OSV)/$(NAME)-$(VERSION).partial
