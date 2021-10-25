#! /bin/sh
. "${SRCROOT}/xcscripts/include.sh"
xmkdir $(dst /private/etc/ssh/sshd_config.d)
ed -s $(dst /private/etc/ssh/sshd_config) <<'EOF'
/strategy
/^$
a
# This Include directive is not part of the default sshd_config shipped with
# OpenSSH. Options set in the included configuration files generally override
# those that follow.  The defaults only apply to options that have not been
# explicitly set.  Options that appear multiple times keep the first value set,
# unless they are a multivalue option such as HostKey.
Include /etc/ssh/sshd_config.d/*

.
g/sftp-server/s/^/#/
wq
EOF
