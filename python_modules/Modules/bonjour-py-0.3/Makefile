NAME = bonjour-py
VERSION = 0.3

PYTHON3 := $(shell python -c 'import sys;print("YES" if sys.version_info[0] > 2 else "NO")')

include ../Makefile.inc

custompatching:
ifeq ($(PYTHON3),YES)
	ed - $(DIR)/setup.py < setup.py3.ed
	ed - $(DIR)/bonjour.i < bonjour.i3.ed
	ed - $(DIR)/examples/browse.py < examples_browse.py3.ed
	ed - $(DIR)/examples/query.py < examples_query.py3.ed
	2to3 --no-diffs -w -n $(DIR)
else
	ed - $(DIR)/setup.py < setup.py.ed
endif

postinstall:
	cp -f $(DIR)/COPYING.txt $(OSL)/$(NAME)-$(VERSION).txt
	cp -f $(NAME).partial $(OSV)/$(NAME)-$(VERSION).partial
