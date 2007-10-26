#
# $Id: Makefile.in,v 1.3.8.1.2.1 2006/02/10 19:47:15 nbk Exp $
#

TARGET     = rlm_ldap
SRCS       = rlm_ldap.c 
HEADERS    =
RLM_CFLAGS =    -DHAVE_LDAP_START_TLS -DHAVE_LDAP_INITIALIZE
RLM_LIBS   = -lldap_r 

include ../rules.mak

$(LT_OBJS): $(HEADERS)
