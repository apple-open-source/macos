/* $OpenLDAP: pkg/ldap/servers/slapd/back-passwd/back-passwd.h,v 1.1.2.1 2002/05/22 14:25:55 kurt Exp $ */
#ifndef _BACK_PASSWD_H
#define _BACK_PASSWD_H

#include "external.h"

LDAP_BEGIN_DECL

extern ldap_pvt_thread_mutex_t passwd_mutex;

LDAP_END_DECL

#endif /* _BACK_PASSWD_H */
