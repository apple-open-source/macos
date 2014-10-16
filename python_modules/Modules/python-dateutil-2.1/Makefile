NAME = python-dateutil
VERSION = 2.1

include ../Makefile.inc

custompatching:
	ed - $(DIR)/setup.py < setup.py.ed

postinstall:
	cp -f $(DIR)/LICENSE $(OSL)/$(NAME)-$(VERSION).txt
	cp -f $(NAME).partial $(OSV)/$(NAME)-$(VERSION).partial
