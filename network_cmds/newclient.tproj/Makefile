#
#  Stubbed Legacy Makefile for the newclient script
#

NAME = newclient

OTHERSRCS = Makefile Makefile.preamble newclient.csh

MAKEFILEDIR = $(MAKEFILEPATH)/project
MAKEFILE = common.make

# INSTALL is already set to "install -S"
XINSTALL = install

INSTALLDIR=/usr/sbin

-include Makefile.preamble

include $(MAKEFILEDIR)/$(MAKEFILE)

-include Makefile.postamble

-include Makefile.dependencies

all project $(NAME):

install:
	@dstdir=$(DSTROOT)`grep NEXTSTEP_INSTALLDIR PB.project | sed 's/.*= \(.*\);.*/\1/'`; \
	if [ ! -d $$dstdir ]; then \
	    $(MKDIRS) $$dstdir; \
	fi; \
	$(RM) -f $$dstdir/$(NAME); \
	echo 	$(XINSTALL) -c -o $(INSTALL_AS_USER) -g $(INSTALL_AS_GROUP) \
		-m 555 newclient.csh $$dstdir/$(NAME); \
	$(XINSTALL) -c -o $(INSTALL_AS_USER) -g $(INSTALL_AS_GROUP) \
		-m 555 newclient.csh $$dstdir/$(NAME)
