NAME = bdist_mpkg
VERSION = 0.4.4

include ../Makefile.inc

postinstall:
	cp -f MIT.license $(OSL)/$(NAME)-$(VERSION).txt
	cp -f $(NAME).partial $(OSV)/$(NAME)-$(VERSION).partial
