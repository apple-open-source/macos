# $OpenLDAP: pkg/ldap/build/rules.mk,v 1.9 2002/01/04 20:17:29 kurt Exp $
## Copyright 1998-2002 The OpenLDAP Foundation
## COPYING RESTRICTIONS APPLY.  See COPYRIGHT File in top level directory
## of this package for details.
##---------------------------------------------------------------------------
##
## Makefile Template for Programs
##

all-common: $(PROGRAMS) FORCE

clean-common: 	FORCE
	$(RM) $(PROGRAMS) $(XPROGRAMS) $(XSRCS) *.o *.lo a.out core *.core \
		    .libs/* *.exe

depend-common: FORCE
	$(MKDEP) $(DEFS) $(DEFINES) $(SRCS)

lint: FORCE
	$(LINT) $(DEFS) $(DEFINES) $(SRCS)

lint5: FORCE
	$(5LINT) $(DEFS) $(DEFINES) $(SRCS)

Makefile: $(top_srcdir)/build/rules.mk

