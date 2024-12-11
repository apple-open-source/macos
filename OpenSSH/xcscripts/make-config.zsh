#!/bin/zsh

#
# Generates all configuration files and outputs them as a bzipped tar
# archive with appropriate ownership and permissions.
#
set -e
setopt pipefail

mkdir -p ${DERIVED_FILE_DIR}


#
# ssh_config, sshd_config
#
include='

# NOTE: The following Include directive is not part of the default
# sshd_config shipped with OpenSSH. Options set in the included
# configuration files generally override those that follow. The defaults
# only apply to options that have not been explicitly set. Options that
# appear multiple times keep the first value set, unless they are a
# multivalue option such as HostKey or IdentityFile.
Include /etc/ssh/@@@_config.d/*'
include=( "${(@f)${include}}" )  # split into lines
include=${(pj.\\\n.)include}     # rejoin lines with trailing backslashes

<${SRCROOT}/openssh/ssh_config sed -E                                      \
    -e "/defaults at the end/a${include//@@@/ssh}"                         \
    >${DERIVED_FILE_DIR}/ssh_config

<${SRCROOT}/openssh/sshd_config sed -E                                     \
    -e "/^# default value/a${include//@@@/sshd}"                           \
    >${DERIVED_FILE_DIR}/sshd_config

#
# tar archive construction
#

# Xcode likes directories with spaces in them, and mtree uses unvis().
src=$(print -rn -- ${SRCROOT} | vis -ctw)
tmp=$(print -rn -- ${DERIVED_FILE_DIR} | vis -ctw)

cat >${DERIVED_FILE_DIR}/ssh.mtree <<EOF
#mtree
/set uid=0 gid=0 mode=644 type=file time=$(date +%s)
private type=dir mode=755
    etc type=dir mode=755

        ssh type=dir mode=755

            crypto type=dir mode=755
                apple.conf content=${src}/conf/apple.conf
                fips.conf content=${src}/conf/fips.conf
                ..
            crypto.conf type=link link=crypto/apple.conf

            moduli contents=${src}/openssh/moduli

            ssh_config content=${tmp}/ssh_config
            ssh_config.d type=dir mode=755
                100-macos.conf content=${src}/conf/100-macos-ssh.conf
                ..

            sshd_config content=${tmp}/sshd_config
            sshd_config.d type=dir mode=755
                100-macos.conf content=${src}/conf/100-macos.conf
EOF

tar -jcf - @${DERIVED_FILE_DIR}/ssh.mtree
