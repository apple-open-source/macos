divert(-1)
#
# Copyright (c) 1998, 1999 Sendmail, Inc. and its suppliers.
#	All rights reserved.
# Copyright (c) 1983 Eric P. Allman.  All rights reserved.
# Copyright (c) 1988, 1993
#	The Regents of the University of California.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)dnl

###
# This file provides a very generic configuration for sendmail.
#
# To customize your configuration, you probably don't want to edit this file.
# See the files in /usr/share/sendmail/conf for more information on how to
# generate a new one with the features and values you want. The file README
# in that directory has instructions.
###

VERSIONID(`$Id: generic-darwin.mc,v 1.3 2002/04/12 18:41:47 bbraun Exp $')
OSTYPE(darwin)dnl
DOMAIN(generic)dnl
undefine(`ALIAS_FILE')
define(`PROCMAIL_MAILER_PATH',`/usr/bin/procmail')
FEATURE(`smrsh',`/usr/libexec/smrsh')
FEATURE(local_procmail)
FEATURE(`virtusertable',`hash -o /etc/mail/virtusertable')dnl
FEATURE(`genericstable', `hash -o /etc/mail/genericstable')dnl 
FEATURE(`mailertable',`hash -o /etc/mail/mailertable')dnl
FEATURE(`access_db')dnl
MAILER(smtp)
MAILER(procmail)
