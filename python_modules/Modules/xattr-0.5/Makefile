NAME = xattr
VERSION = 0.5

SETUPARGS = --install-scripts=/usr/bin

include ../Makefile.inc

custompatching:
	cp -f rtool.py $(DIR)/Lib/xattr/tool.py
	pwd=`pwd` && cd $(DIR) && patch -p0 -i $$pwd/KeyError.diff

postinstall:
	cp -f $(DIR)/LICENSE.txt $(OSL)/$(NAME)-$(VERSION).txt
	cp -f $(NAME).partial $(OSV)/$(NAME)-$(VERSION).partial
