#
# $Id: Makefile.in,v 1.5.2.1.2.2 2006/02/10 19:47:14 nbk Exp $
#

TARGET      = rlm_eap_ttls
SRCS        = rlm_eap_ttls.c ttls.c
HEADERS     = eap_ttls.h ../../eap.h ../../rlm_eap.h
RLM_CFLAGS  = -I../.. -I../../libeap $(INCLTDL) $(OPENSSL_INCLUDE) 
RLM_LIBS    =  ../../libeap/libeap.la $(OPENSSL_LIBS)
RLM_INSTALL =

RLM_DIR=../../
include ${RLM_DIR}../rules.mak

$(LT_OBJS): $(HEADERS)
