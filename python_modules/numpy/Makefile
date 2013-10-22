NAME = numpy
VERSION = 1.6.2

include ../Makefile.inc

custompatching:
	ed - $(DIR)/numpy/distutils/system_info.py < system_info.py.ed

postinstall:
	cp -f $(DIR)/LICENSE.txt $(OSL)/$(NAME)-$(VERSION).txt
	cp -f $(NAME).partial $(OSV)/$(NAME)-$(VERSION).partial
