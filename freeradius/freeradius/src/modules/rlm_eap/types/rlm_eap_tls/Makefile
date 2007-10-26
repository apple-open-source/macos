#
# $Id: Makefile.in,v 1.5.4.2 2006/02/10 19:47:13 nbk Exp $
#

TARGET      = rlm_eap_tls
SRCS        = rlm_eap_tls.c
HEADERS     = rlm_eap_tls.h ../../eap.h ../../rlm_eap.h
RLM_CFLAGS  = -I../.. -I../../libeap $(INCLTDL) $(OPENSSL_INCLUDE) 
RLM_LIBS    =  ../../libeap/libeap.la $(OPENSSL_LIBS)
RLM_INSTALL =

RLM_DIR=../../
include ${RLM_DIR}../rules.mak

$(LT_OBJS): $(HEADERS)
