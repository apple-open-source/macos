# matplotlib
#
# TMPPREFIX contains prebuilt libpng.

NAME = matplotlib
VERSION = 1.3.1

include ../Makefile.inc

custompatching:
	cp -f $(DIR)/setup.cfg.template $(DIR)/setup.cfg
	ed - $(DIR)/setup.py < setup.py.ed
	ed - $(DIR)/setupext.py < setupext.py.ed
	sed 's,XXXXX,$(TMPPREFIX),' setup.cfg.ed | ed - $(DIR)/setup.cfg
	ed - $(DIR)/lib/matplotlib/animation.py < lib_matplotlib_animation.py.ed
	ed - $(DIR)/src/ft2font.cpp < error.ed

postinstall:
	cp -f LICENSE $(OSL)/$(NAME)-$(VERSION).txt
	cp -f $(NAME).partial $(OSV)/$(NAME)-$(VERSION).partial
	cp -f $(SRCROOT)/Modules/$(NAMEVERSION)/CHANGES $(DSTROOT)$(EXTRASPYTHON)/$(NAME)
