# This file, along with the "strip" perl script, works around a verification
# error caused by a UFS bug (stripping a multi-link file breaks the link, and
# sometimes causes the wrong file to be stripped/unstripped).  By using the
# "strip" perl script, it not only causes the correct file to be stripped, but
# also preserves the link.

export PATH:=$(SRCROOT)/bin:$(PATH)

.DEFAULT:
	@$(MAKE) -f Makefile $@
