NAME = xattr
VERSION = 0.6.4


PYTHON3 := $(shell python -c 'import sys;print("YES" if sys.version_info[0] > 2 else "NO")')
SETUPARGS = --install-scripts=/usr/bin

include ../Makefile.inc

custompatching:
	ed - $(DIR)/xattr/__init__.py < xattr___init__.py.ed
	ed - $(DIR)/xattr/_xattr.c < xattr__xattr.c.ed
ifeq ($(PYTHON3),YES)
	ed - $(DIR)/xattr/__init__.py < xattr___init__.py3.ed
	! { unifdef -DPY3K -o $(DIR)/xattr/tool.py rtool.py.in || \
	[ $$? -ne 1 ]; }
	2to3 --no-diffs -w -n $(DIR)
else
	! { unifdef -UPY3K -o $(DIR)/xattr/tool.py rtool.py.in || \
	[ $$? -ne 1 ]; }
endif

postinstall:
	cp -f $(DIR)/LICENSE.txt $(OSL)/$(NAME)-$(VERSION).txt
	cp -f $(NAME).partial $(OSV)/$(NAME)-$(VERSION).partial
	mkdir -p $(DSTROOT)/usr/share/man/man1
	cp -f $(NAME).1 $(DSTROOT)/usr/share/man/man1
