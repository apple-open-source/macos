NAME = macholib
VERSION = 1.2.1.dev-r432

include ../Makefile.inc

custompatching:
	(cd $(DIR) && patch -p1) < patches/dyld_info_only.diff
	ex - $(DIR)/macholib/mach_o.py < mach_o.py.ex

postinstall:
	cp -f MIT.license $(OSL)/$(NAME).txt
	cp -f $(NAME).partial $(OSV)
