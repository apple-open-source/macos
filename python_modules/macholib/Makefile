NAME = macholib
VERSION = 1.5

include ../Makefile.inc

custompatching:
	ed - $(DIR)/macholib/MachO.py < MachO.py.ed
	ed - $(DIR)/macholib/mach_o.py < mach_o.py.ed

postinstall:
	cp -f MIT.license $(OSL)/$(NAME)-$(VERSION).txt
	cp -f $(NAME).partial $(OSV)/$(NAME)-$(VERSION).partial
