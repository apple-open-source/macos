#!/bin/sh

USER="$1"
_quota_txt=/etc/mail/quota_exceeded.txt

if [ -e $_quota_txt ]; then
  cat $_quota_txt | /usr/libexec/dovecot/dovecot-lda -d "$USER" -o "plugin/quota=maildir:User quota:noenforcing"
fi
