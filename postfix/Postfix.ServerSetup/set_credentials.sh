#!/bin/sh
#
# Set credentials for using URLAUTH with IMAP servers.
#

/usr/sbin/postconf -e imap_submit_cred_file=/Library/Server/Mail/Config/postfix/submit.cred

# Create submit.cred with either the same password dovecot is
# configured for, or an unguessable random password.
if [ ! -e /Library/Server/Mail/Config/postfix/submit.cred ] ; then
	hostname=`/usr/sbin/postconf -c /Library/Server/Mail/Config/postfix -h myhostname`
	if [ ! "$hostname" ] ; then
		hostname=`hostname`
	fi

	# get pw from existing dovecot submit.passdb
	if [ -s /Library/Server/Mail/Config/dovecot/submit.passdb ] ; then
		pw=`grep "^submit:" /private/etc/dovecot/submit.passdb | sed -e 's,.*},,' -e 's,:.*,,'`
	elif [ -s /private/etc/dovecot/submit.passdb ] ; then
		pw=`grep "^submit:" /Library/Server/Mail/Config/dovecot/submit.passdb | sed -e 's,.*},,' -e 's,:.*,,'`
	fi

	#  if no existing pw, create a new one
	if [ ! "$pw" ] ; then
		pw=`dd if=/dev/urandom bs=256 count=1 2>&1 | env LANG=C tr -dc a-zA-Z0-9 2>&1 | cut -b 1-22 2>&1`
	fi

	# set submit.cred with host name & pw
	if [ "$pw" -a "$hostname" ]; then
		echo "submitcred version 1" > /Library/Server/Mail/Config/postfix/submit.cred
		echo "$hostname|submit|$pw" >> /Library/Server/Mail/Config/postfix/submit.cred
	fi
	chmod 600 /Library/Server/Mail/Config/postfix/submit.cred
fi
