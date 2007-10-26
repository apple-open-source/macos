# -*- makefile -*-
# Make.inc.in
#
# Version:	$Id: Make.inc.in,v 1.48.4.3 2006/07/15 17:15:59 nbk Exp $
#

# Location of files.
prefix		= /usr
exec_prefix	= ${prefix}
sysconfdir	= /private/etc
localstatedir	= /private/var
libdir		= ${exec_prefix}/lib
bindir		= ${exec_prefix}/bin
sbindir		= ${exec_prefix}/sbin
docdir		= ${datadir}/doc/freeradius
mandir		= /usr/share/man
datadir		= ${prefix}/share
dictdir		= $(datadir)/freeradius
logdir		= ${localstatedir}/log/radius
raddbdir	= ${sysconfdir}/raddb
radacctdir	= ${logdir}/radacct
top_builddir	= /Volumes/Tenacious/Staging/freeradius-1.1.3-make
top_srcdir	= /Volumes/Tenacious/Staging/freeradius-1.1.3-make

MAKE		= /usr/bin/make
CC		= gcc
RANLIB		= ranlib
INCLUDE		= 
CFLAGS		= $(INCLUDE) -g -O2 -D_REENTRANT -D_POSIX_PTHREAD_SEMANTICS -DDARWIN -Wall -D_GNU_SOURCE -DNDEBUG 
CPPFLAGS	= 
LIBPREFIX	= lib
EXEEXT		=  

LIBTOOL		= /Volumes/Tenacious/Staging/freeradius-1.1.3-make/libtool
ACLOCAL		= aclocal
AUTOCONF	= autoconf
AUTOHEADER	= autoheader
INSTALL		= ${top_builddir}/install-sh -c
INSTALL_PROGRAM	= ${INSTALL}
INSTALL_DATA	= ${INSTALL} -m 644
INSTALL_SCRIPT	= ${INSTALL_PROGRAM}
INSTALLSTRIP	= -s

LCRYPT		= 
LIBS		= -lresolv  -lpthread 
LDFLAGS		=  

LOGDIR		= ${logdir}
RADDBDIR	= ${raddbdir}
RUNDIR		= ${localstatedir}/run/radiusd
SBINDIR		= ${sbindir}
RADIR		= ${radacctdir}

LIBLTDL		= -lltdl
INCLTDL		= 
LTDL_SUBDIRS	= 

USE_SHARED_LIBS	= yes
USE_STATIC_LIBS = yes
STATIC_MODULES	= 

SNMP_INCLUDE	= 
SNMP_LIBS	= 

OPENSSL_LIBS	=  -lcrypto -lssl -lcrypto
OPENSSL_INCLUDE = -DOPENSSL_NO_KRB5

#
#  SQL driver defines
#
SQL_ODBC_LIBS = @ODBC_LIBS@
SQL_ODBC_INCLUDE = @ODBC_INCLUDE@

RADIUSD_MAJOR_VERSION	= 1
RADIUSD_MINOR_VERSION	= 1.3
RADIUSD_VERSION		= 1.1.3

MODULES			=  rlm_acct_unique rlm_always rlm_attr_filter rlm_attr_rewrite rlm_chap rlm_counter rlm_dbm rlm_detail rlm_digest rlm_eap rlm_exec rlm_expr rlm_fastusers rlm_files rlm_ippool rlm_krb5 rlm_ldap rlm_mschap rlm_ns_mta_md5 rlm_otp rlm_pam rlm_pap rlm_passwd rlm_preprocess rlm_radutmp rlm_realm rlm_sql rlm_sql_log rlm_sqlcounter rlm_unix rlm_checkval
HOSTINFO		= powerpc-apple-darwin9.0.0d3
