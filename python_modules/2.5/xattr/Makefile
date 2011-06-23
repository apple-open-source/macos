NAME = xattr
VERSION = 0.5

SETUPARGS = --install-scripts=/usr/bin

include ../Makefile.inc

custompatching:
	ed - $(DIR)/Lib/xattr/tool.py < tool.py.ed
	cd $(DIR) && patch -p0 -i $(SRCROOT)/$(NAME)/KeyError.diff

postinstall:
	cp -f rtool.py $(DSTROOT)/usr/bin/$(NAME)
	chmod 755 $(DSTROOT)/usr/bin/$(NAME)
	cp -f $(DIR)/LICENSE.txt $(OSL)/$(NAME)-$(VERSION).txt
	cp -f $(NAME).partial $(OSV)/$(NAME)-$(VERSION).partial
