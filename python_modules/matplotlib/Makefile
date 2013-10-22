# matplotlib
#
# TMPPREFIX contains prebuilt libpng.

NAME = matplotlib
VERSION = 1.1.1

include ../Makefile.inc

custompatching:
	cp -f $(DIR)/setup.cfg.template $(DIR)/setup.cfg
	sed 's,XXXXX,$(TMPPREFIX),' setup.cfg.ed | ed - $(DIR)/setup.cfg
	ed - $(DIR)/setupext.py < setupext.py.ed
	ed - $(DIR)/lib/matplotlib/animation.py < lib_matplotlib_animation.py.ed
	ed - $(DIR)/src/ft2font.cpp < error.ed

postinstall:
	cp -f LICENSE $(OSL)/$(NAME)-$(VERSION).txt
	cp -f $(NAME).partial $(OSV)/$(NAME)-$(VERSION).partial
