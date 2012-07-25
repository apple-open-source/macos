#!/bin/sh
#
# Set credentials for using URLAUTH with IMAP servers.
#

/usr/sbin/postconf -e imap_submit_cred_file=/Library/Server/Mail/Config/postfix/submit.cred

# Create submit.cred with either the same password dovecot is
# configured for, or an unguessable random password.
if [ ! -e /Library/Server/Mail/Config/postfix/submit.cred ] ; then
	hostname=`grep "^myhostname *=" /Library/Server/Mail/Config/postfix/main.cf | sed 's,.*= *,,'`
	if [ ! "$hostname" ] ; then
		hostname=`hostname`
	fi
	if [ -s /private/etc/dovecot/submit.passdb ] ; then
		pw=`grep "^submit:" /private/etc/dovecot/submit.passdb | sed 's,.*},,'`
	fi
	if [ ! "$pw" ] ; then
		pw=`dd if=/dev/urandom bs=256 count=1 | env LANG=C tr -dc a-zA-Z0-9 | cut -b 1-22`
	fi
	if [ "$pw" -a "$hostname" ]; then
		echo "submitcred version 1" > /Library/Server/Mail/Config/postfix/submit.cred
		echo "$hostname|submit|$pw" >> /Library/Server/Mail/Config/postfix/submit.cred
	fi
	chmod 600 /Library/Server/Mail/Config/postfix/submit.cred
fi
