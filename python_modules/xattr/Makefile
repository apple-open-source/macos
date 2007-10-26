NAME = xattr
VERSION = 0.5

SETUPARGS = --install-scripts=/usr/bin

include ../Makefile.inc

custompatching:
	ed - $(DIR)/Lib/xattr/tool.py < tool.py.ed
