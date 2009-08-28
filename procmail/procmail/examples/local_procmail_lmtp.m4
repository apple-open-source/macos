divert(-1)
#	Copyright 2001, Philip Guenther, The United States of America
#
#	This file should be copied to the cf/feature directory of the
#	sendmail distribution.	The following blurb is roughly what would
#	go in the list of FEATURES in the cf/README file:
#
# `local_procmail_lmtp
#		Use procmail as an LMTP capable local mailer.  By using
#		LMTP for the connection between sendmail and procmail,
#		delivery to multiple recipients can be performed more
#		efficiently while still allowing a separate status code
#		for each recipient.  With this feature, the local mailer
#		can make use of the "user+indicator@local.host" syntax;
#		normally the +indicator is just tossed, but by default
#		it is passed as the -a argument to procmail.
#
#		This feature can take up to three arguments:
#		1. Path to the mailer program
#		   [default: PROCMAIL_MAILER_PATH or /usr/local/bin/procmail]
#		2. Argument vector including name of the program
#		   [default: procmail -Y -a $h -z]
#		3. Flags for the mailer [default: PSXhmnz9]
#
#		Empty arguments cause the defaults to be taken.
#		WARNING: This feature sets LOCAL_MAILER_FLAGS unconditionally,
#		i.e.,  without respecting any definitions in an OSTYPE setting.'
#
#	This feature would probably be better implemented on top of the
#	local_lmtp or local_procmail features that are currently distributed,
#	but the following is known to work with Sendmail 8.11
divert(0)
VERSIONID(`$Id: local_procmail_lmtp.m4,v 1.3 2001/09/11 04:45:48 guenther Exp $')
divert(-1)

define(`LOCAL_MAILER_PATH',
	ifelse(defn(`_ARG_'), `',
		ifdef(`PROCMAIL_MAILER_PATH',
			PROCMAIL_MAILER_PATH,
			`/usr/local/bin/procmail'),
		_ARG_))
define(`LOCAL_MAILER_ARGS',
	ifelse(len(X`'_ARG2_), `1', `procmail -Y -a $h -z', _ARG2_))
define(`LOCAL_MAILER_FLAGS',
	ifelse(len(X`'_ARG3_), `1', `PSXhmnz9', _ARG3_))
define(`LOCAL_MAILER_DSN_DIAGNOSTIC_CODE', `SMTP')
