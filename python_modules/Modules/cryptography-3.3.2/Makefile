NAME = cryptography
VERSION = 3.3.2

include ../Makefile.inc

custompatching:
	( cd $(DIR) ; patch -p1 ) < cryptography-build-with-libressl-3.3.2.patch
	( cd $(DIR) ; patch -p1 ) < libressl-3.3-default.patch

postinstall:
	cp -f $(DIR)/LICENSE.APACHE $(OSL)/$(NAME)-$(VERSION).txt
	cp -f $(NAME).partial $(OSV)/$(NAME)-$(VERSION).partial
