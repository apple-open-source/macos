NAME = bdist_mpkg
VERSION = 0.4.4

PYTHON3 := $(shell python -c 'import sys;print("YES" if sys.version_info[0] > 2 else "NO")')

include ../Makefile.inc

ifeq ($(PYTHON3),YES)
custompatching:
	2to3 --no-diffs -w -n $(DIR)
endif

postinstall:
	cp -f MIT.license $(OSL)/$(NAME)-$(VERSION).txt
	cp -f $(NAME).partial $(OSV)/$(NAME)-$(VERSION).partial
