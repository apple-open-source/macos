#
# $Id: Makefile.in,v 1.4.8.1.2.1 2006/02/10 19:48:04 nbk Exp $
#

TARGET      = rlm_dbm
SRCS        = rlm_dbm.c
HEADERS     =
RLM_UTILS   = rlm_dbm_parser rlm_dbm_cat
RLM_CFLAGS  =    -DHAVE_NDBM_H
RLM_LIBS    = 
RLM_INSTALL = rlm_dbm_install

include ../rules.mak

rlm_dbm_parser: rlm_dbm_parser.lo ../../lib/libradius.la
	$(LIBTOOL) --mode=link $(CC) $(LDFLAGS) $(RLM_LDFLAGS) \
		-o $@ $^ $(RLM_LIBS) $(LIBS)

rlm_dbm_cat: rlm_dbm_cat.lo ../../lib/libradius.la
	$(LIBTOOL) --mode=link $(CC) $(LDFLAGS) $(RLM_LDFLAGS) \
		-o $@ $^ $(RLM_LIBS) $(LIBS)

#
#  Install the utility programs in the bin directory.
#
rlm_dbm_install: rlm_dbm_cat rlm_dbm_parser
	$(LIBTOOL) --mode=install $(INSTALL) -m 755 $(INSTALLSTRIP) \
		rlm_dbm_cat$(EXEEXT) $(R)$(bindir)
	$(LIBTOOL) --mode=install $(INSTALL) -m 755 $(INSTALLSTRIP) \
		rlm_dbm_parser$(EXEEXT) $(R)$(bindir)
