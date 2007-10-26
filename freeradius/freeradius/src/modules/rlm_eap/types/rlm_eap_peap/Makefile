#
# $Id: Makefile.in,v 1.2.2.2.2.2 2006/02/10 19:47:12 nbk Exp $
#

TARGET      = rlm_eap_peap
SRCS        = rlm_eap_peap.c peap.c
HEADERS     = eap_peap.h ../../eap.h ../../rlm_eap.h
RLM_CFLAGS  = -I../.. -I../../libeap $(INCLTDL) $(OPENSSL_INCLUDE) 
RLM_LIBS    =  ../../libeap/libeap.la $(OPENSSL_LIBS)
RLM_INSTALL =

RLM_DIR=../../
include ${RLM_DIR}../rules.mak

$(LT_OBJS): $(HEADERS)
