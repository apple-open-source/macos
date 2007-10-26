#!/bin/sh
#
# Remove old cron task and add to launchd if enabled

OLD_CRON=/var/amavis/tmp/root_crontab.old.txt
NEW_CRON=/var/amavis/tmp/root_crontab.new.txt
LEARN_JUNK_MAIL="spamassassin/learn_junk_mail"

crontab -u root -l > $OLD_CRON

SA_STRING=`cat $OLD_CRON | grep $LEARN_JUNK_MAIL`
if [ "$SA_STRING" != "" ]; then
  sed '/spamassassin\/learn_junk_mail/d' $OLD_CRON > $NEW_CRON
  crontab -u root $NEW_CRON
  rm -f $OLD_CRON $NEW_CRON
  service com.apple.learnjunkmail start
else
  rm -f $OLD_CRON
fi
