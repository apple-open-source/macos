#! /bin/sh
. "${SRCROOT}/xcscripts/include.sh"
xmkdir $(dst /private/etc/ssh/ssh_config.d)
ed -s $(dst /private/etc/ssh/ssh_config) <<'EOF'
/defaults at the end
a

# This Include directive is not part of the default ssh_config shipped with
# OpenSSH. Options set in the included configuration files generally override
# those that follow.  The defaults only apply to options that have not been
# explicitly set.  Options that appear multiple times keep the first value set,
# unless they are a multivalue option such as IdentityFile.
Include /etc/ssh/ssh_config.d/*
.
$
a
Host *
    SendEnv LANG LC_*
.
wq
EOF
