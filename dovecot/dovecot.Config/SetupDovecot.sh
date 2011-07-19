#!/bin/sh
#
# Configure mail services
#

################
# Mail directories

_dovecot_mail_dir=/Library/Server/Mail/Data/mail
_dovecot_sieve_dir=/Library/Server/Mail/Data/rules
_mailaccess_log=/var/log/mailaccess.log

################
# Create dovecot.conf file on clean install

(
	cd /etc/dovecot/default
	find -s * -type f -print | while read f
	do
		if [ ! -e ../"$f" ]
		then
			cp -v "$f" ../"$f"
			chmod 644 ../"$f"
		fi
	done
)

hostname=`grep "^myhostname *=" /etc/postfix/main.cf | sed 's,.*= *,,'`
if [ ! "$hostname" ] ; then
  hostname=`hostname`
fi
if [ "$hostname" ] ; then
  perl -p -i -e '
	s/^(\s*)#\s*postmaster_address\s*=.*/$1postmaster_address = postmaster\@'"$hostname"'/;
	s/^(\s*)postmaster_address\s*=\s*postmaster\@example\.com/$1postmaster_address = postmaster\@'"$hostname"'/;
  ' /etc/dovecot/*.conf /etc/dovecot/conf.d/*.conf
fi

# Create submit.passdb with either the same password postfix is configured for,
# or an unguessable random password on clean install.
if [ ! -e /etc/dovecot/submit.passdb ] ; then
  if [ "$hostname" -a -s /etc/postfix/submit.cred ] ; then
    pw=`grep "^$hostname|submit|" /etc/postfix/submit.cred | sed 's,.*|,,'`
  fi
  if [ ! "$pw" ] ; then
    pw=`dd if=/dev/urandom bs=256 count=1 | env LANG=C tr -dc a-zA-Z0-9 | cut -b 1-22`
  fi
  if [ "$pw" ] ; then
    echo "submit:{PLAIN}$pw" > /etc/dovecot/submit.passdb
    chown :mail /etc/dovecot/submit.passdb
    chmod 640 /etc/dovecot/submit.passdb
  fi
fi

###############
# Verify dovecot default directories

if [ ! -d $_dovecot_mail_dir ]; then
  mkdir -p $_dovecot_mail_dir
fi

if [ -d $_dovecot_mail_dir ]; then
  chmod 775 $_dovecot_mail_dir
  chown _dovecot:mail $_dovecot_mail_dir
fi

if [ ! -d $_dovecot_sieve_dir ]; then
  mkdir -p $_dovecot_sieve_dir
fi

if [ -d $_dovecot_sieve_dir ]; then
  chmod 775 $_dovecot_sieve_dir
  chown _dovecot:mail $_dovecot_sieve_dir
fi

if [ ! -e $_mailaccess_log ]; then
  touch $_mailaccess_log
fi

if [ -e $_mailaccess_log ]; then
  chmod 640 $_mailaccess_log
  chown _dovecot:admin $_mailaccess_log
fi

if [ ! -d /var/db/dovecot.fts.update ]; then
  mkdir -p -m 770 /var/db/dovecot.fts.update
  chown _dovecot:mail /var/db/dovecot.fts.update
fi

if ! grep '^local6\.' /etc/syslog.conf >/dev/null
then
  echo "local6.warn\t\t\t\t\t\t$_mailaccess_log" >> /etc/syslog.conf
  kill -1 `cat /var/run/syslog.pid`
fi
