/* acconfig.h
   This file is in the public domain.

   Descriptive text for the C preprocessor macros that
   the distributed Autoconf macros can define.
   No software package will use all of them; autoheader copies the ones
   your configure.in uses into your configuration header file templates.

   The entries are in sort -df order: alphabetical, case insensitive,
   ignoring punctuation (such as underscores).  Although this order
   can split up related entries, it makes it easier to check whether
   a given entry is in the file.

   Leave the following blank line there!!  Autoheader needs it.  */


/* Define to 1 if NLS is requested.  */
#undef ENABLE_NLS

/* Define as 1 if you have catgets and don't want to use GNU gettext.  */
#undef HAVE_CATGETS

/* Define as 1 if you have gettext and don't want to use GNU gettext.  */
#undef HAVE_GETTEXT

/* Define if you have herror available in your bind library */
#undef HAVE_HERROR

/* Define if your locale.h file contains LC_MESSAGES.  */
#undef HAVE_LC_MESSAGES

/* Define if you have res_search available in your bind library */
#undef HAVE_RES_SEARCH

/* Define as 1 if you have the stpcpy function.  */
#undef HAVE_STPCPY

/* Define if your C compiler allows void * as a function result */
#undef HAVE_VOIDPOINTER

/* Define if your C compiler allows ANSI volatile */
#undef HAVE_VOLATILE

/* Define if `union wait' is the type of the first arg to wait functions.  */
#undef HAVE_UNION_WAIT

/* Define if you have the strcasecmp function.  */
#undef HAVE_STRCASECMP

/* Define if you have the memmove function.  */
#undef HAVE_MEMMOVE

/* Define if you have GNU's getopt family of functions.  */
#undef HAVE_GETOPTLONG

/* Define if you have strstr */
#undef HAVE_STRSTR

/* Define if you have vsyslog */
#undef HAVE_VSYSLOG

/* Define if you have atexit */
#undef HAVE_ATEXIT

/* Define if you have on_exit */
#undef HAVE_ON_EXIT

/* Define if you have setrlimit */
#undef HAVE_SETRLIMIT

/* Define to the name of the distribution.  */
#undef PACKAGE

/* Compute an appropriate directory for PID lock files */
#undef PID_DIR

/* Define to help us deduce a 32-bit type (required for Kerberos) */
#undef SIZEOF_INT
#undef SIZEOF_SHORT
#undef SIZEOF_LONG

/* Define if you want POP2 support compiled in */
#undef POP2_ENABLE

/* Define if you want POP3 support compiled in */
#undef POP3_ENABLE

/* Define if you want IMAP support compiled in */
#undef IMAP_ENABLE

/* Define if you want ETRN support compiled in */
#undef ETRN_ENABLE

/* Define if you want RPA support compiled in */
#undef RPA_ENABLE

/* Define if you want NTLM authentication */
#undef NTLM_ENABLE

/* Define if you want SDPS support compiled in */
#undef SDPS_ENABLE

/* Define if you want OPIE support compiled in */
#undef OPIE

/* Define if you want IPv6 support compiled in */
#undef INET6

/* Define if you want network security support compiled in */
#undef NET_SECURITY

/* Define if you want GSSAPI authentication */
#undef GSSAPI

/* Define if you have HEIMDAL kerberos 5 */
#undef HEIMDAL

/* Define if you have MIT kerberos */
#undef HAVE_GSS_C_NT_HOSTBASED_SERVICE

/* Define if you want built-in SOCKS support */
#undef HAVE_SOCKS

/* Define to the version of the distribution.  */
#undef VERSION


/* Leave that blank line there!!  Autoheader needs it.
   If you're adding to this file, keep in mind:
   The entries are in sort -df order: alphabetical, case insensitive,
   ignoring punctuation (such as underscores).  */

