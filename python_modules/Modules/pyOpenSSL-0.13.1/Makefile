NAME = pyOpenSSL
VERSION = 0.13.1

include ../Makefile.inc

custompatching:
	patch -p0 $(DIR)/OpenSSL/rand/rand.c pyOpenSSL.diff

postinstall:
	cp -f $(DIR)/LICENSE $(OSL)/$(NAME)-$(VERSION).txt
	cp -f $(NAME).partial $(OSV)/$(NAME)-$(VERSION).partial
