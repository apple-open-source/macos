#!/bin/bash 
/usr/bin/spamassassin | /usr/sbin/sendmail -i "$@" 
exit $? 
