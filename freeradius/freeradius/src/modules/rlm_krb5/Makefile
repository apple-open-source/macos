#
# $Id: Makefile.in,v 1.1.8.1.2.1 2006/02/10 19:47:15 nbk Exp $
#

TARGET     = rlm_krb5
SRCS       = rlm_krb5.c
HEADERS    =
RLM_LIBS   = -lkrb5 -lcom_err -lcrypto -lk5crypto 
RLM_CFLAGS =     -I/usr/include/et

include ../rules.mak

$(LT_OBJS): $(HEADERS)
