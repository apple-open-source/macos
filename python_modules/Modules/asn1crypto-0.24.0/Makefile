NAME = asn1crypto
VERSION = 0.24.0

include ../Makefile.inc

custompatching:
	patch -d $(DIR) -p1 < dlopen_versioned.patch

postinstall:
	cp -f $(DIR)/LICENSE $(OSL)/$(NAME)-$(VERSION).txt
	cp -f $(NAME).partial $(OSV)/$(NAME)-$(VERSION).partial
