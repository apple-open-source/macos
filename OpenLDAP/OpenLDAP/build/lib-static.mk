# $OpenLDAP: pkg/ldap/build/lib-static.mk,v 1.7 2002/02/10 21:42:51 kurt Exp $
## Copyright 1998-2002 The OpenLDAP Foundation
## COPYING RESTRICTIONS APPLY.  See COPYRIGHT File in top level directory
## of this package for details.
##---------------------------------------------------------------------------
##
## Makefile Template for Static Libraries
##

$(LIBRARY): version.o
	$(AR) ru $@ $(OBJS) version.o
	@$(RANLIB) $@

Makefile: $(top_srcdir)/build/lib-static.mk
