NAME = future
VERSION = 0.17.1

include ../Makefile.inc

postinstall:
	cp -f $(DIR)/LICENSE.txt $(OSL)/$(NAME)-$(VERSION).txt
	cp -f $(NAME).partial $(OSV)/$(NAME)-$(VERSION).partial
	echo "# " > $(DSTROOT)$(EXTRASPYTHON)/future/backports/test/nullcert.pem
