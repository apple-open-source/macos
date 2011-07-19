NAME = macholib
VERSION = 1.3

include ../Makefile.inc

custompatching:
	ed - $(DIR)/macholib/MachO.py < MachO.py.ed
	ex - $(DIR)/macholib/mach_o.py < mach_o.py.ex

postinstall:
	cp -f MIT.license $(OSL)/$(NAME)-$(VERSION).txt
	cp -f $(NAME).partial $(OSV)/$(NAME)-$(VERSION).partial
