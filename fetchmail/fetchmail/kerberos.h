/*
 * kerberos.h -- Headers for Kerberos support.
 *
 * For license terms, see the file COPYING in this directory.
 */

#ifndef H_KERBEROS__
#define H_KERBEROS__
#include  "config.h"
#if defined(KERBEROS_V4) || defined(KERBEROS_V5)

#ifdef KERBEROS_V5
#include <krb5.h>
/* #include <com_err.h> */
#endif

#ifdef KERBEROS_V4
#  ifdef KERBEROS_V4_V5
#    include <kerberosIV/krb.h>
#    include <kerberosIV/des.h>
#  else
#    if defined (__bsdi__) 
#      include <des.h> /* order of includes matters */
#      define krb_get_err_text(e) (krb_err_txt[e])
#    endif
#    include <krb.h>
#    if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__linux__)
#      define krb_get_err_text(e) (krb_err_txt[e])
#      include <des.h>
#    endif
#  endif
#endif

/* des.h might define _ for no good reason.  */
#undef _

#endif /* KERBEROS_V4 || KERBEROS_V5 */
#endif /* H_KERBEROS__ */
