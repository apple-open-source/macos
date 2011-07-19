#!/bin/sh
#
# Configure SpamAssassin
#
echo "Configuring junk mail scanner..."

# Install config files
_config_file="/etc/mail/spamassassin/local.cf"
_default_config="/etc/mail/spamassassin/local.cf.default"
if [ ! -e $_config_file ]; then
    if [ -e $_default_config ]; then
      cp $_default_config $_config_file
    fi
fi

chown root:wheel $_config_file
chmod 644 $_config_file

