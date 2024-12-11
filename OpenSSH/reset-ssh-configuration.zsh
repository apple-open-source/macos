#! /bin/zsh

setopt pipefail

if (( ${EUID} )) {
    print -Pr -- "%F{red}${0:t} must be run using sudo.%f"
    exit 1
}
print -Pr -- "%F{yellow}\
WARNING: Continuing will overwrite the existing SSH configuration
         files in /etc/ssh with the macOS default configuration.
         Only host keys (files named \`ssh_host_*_key*\`) will be
         preserved.%f"
print -Prn -- "%F{cyan}\
Do you wish to reset SSH to the macOS defaults? [ (y)es / (n)o ]%f "
read -r
case ${(L)REPLY:-no} in
    y|yes)
        ;;
    *)
        print -Pr -- "%F{cyan}Cancelled." \
            "Leaving existing configuration intact.%f"
        exit 2
        ;;
esac

tarball="/usr/share/openssh/default-configuration.tar.bz2"
tmpdir=$(mktemp -d "/var/tmp/sshconfig.XXXXXX")
if (( ${rc::=$?} )) {
    print -Pru2 -- "%F{red}Could not create temporary directory.%f"
    exit ${rc}
}
backup="${tmpdir}/previous-configuration.tar.bz2"
tar -C / -jcf ${backup} private/etc/ssh
if (( ${rc::=$?} )) {
    print -Pru2 -- "%F{red}Could not backup existing configuration.%f"
    exit ${rc}
}
print -Pr -- "%F{cyan}Preserved existing configuration at:%f"
print -Pr -- "    ${backup}"
tar -tf ${backup} private/etc/ssh             | \
    egrep 'private/etc/ssh/ssh_host_.*_key'   > \
    ${tmpdir}/host_keys
if (( ${rc::=$?} )) {
    print -Pru2 -- "%F{red}Could not identify existing host keys.%f"
    exit ${rc}
}
mv /private/etc/ssh /private/etc/ssh.$$
if (( ${rc::=$?} )) {
    print -Pru2 -- "%F{red}Could not move aside existing configuration.%f"
    exit ${rc}
}
rm -rf /private/etc/ssh.$$  # Ignore failures.
tar -C /private/etc --strip-components 2 -xf ${tarball}
if (( ${rc::=$?} )) {
    print -Pru2 -- "%F{red}Resetting SSH configuration failed.%f"
    exit ${rc}
}
tar -C /private/etc --strip-components 2 -T ${tmpdir}/host_keys -xf ${backup}
if (( ${rc::=$?} )) {
    print -Pru2 -- "%F{red}Could not restore previous host keys.%f"
    exit ${rc}
}
print -Pr -- "%F{green}\
The SSH configuration has been reset to macOS defaults."
exit 0
