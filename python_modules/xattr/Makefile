NAME = xattr
VERSION = 0.6.2

SETUPARGS = --install-scripts=/usr/bin

include ../Makefile.inc

custompatching:
	ed - $(DIR)/xattr/tool.py < tool.py.ed
	cd $(DIR) && patch -p0 -i $(SRCROOT)/$(NAME)/KeyError.diff

postinstall:
	cp -f rtool.py $(DSTROOT)/usr/bin/$(NAME)
	chmod 755 $(DSTROOT)/usr/bin/$(NAME)
	cp -f $(DIR)/LICENSE.txt $(OSL)/$(NAME)-$(VERSION).txt
	cp -f $(NAME).partial $(OSV)/$(NAME)-$(VERSION).partial
	mkdir -p $(DSTROOT)/usr/share/man/man1
	cp -f $(NAME).1 $(DSTROOT)/usr/share/man/man1
