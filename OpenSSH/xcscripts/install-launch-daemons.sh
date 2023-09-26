#!/bin/sh
. "${SRCROOT}/xcscripts/include.sh"

pb() {
    local plist="$1"
    shift
    for cmd in "$@"; do
        /usr/libexec/PlistBuddy -c "${cmd}" "${plist}" || return "$?"
    done
}

system="$(dst /System/Library/LaunchDaemons)"
internal="$(dst /AppleInternal/Library/LaunchDaemons)"
sshplist="$(obj ssh.plist)"
xmkdir ${system} ${internal}

cp $(src com.openssh.sshd.plist) ${sshplist}
pb ${sshplist} \
  'Delete :Disabled' \
  'Add :_PanicOnCrash dict' \
  'Add :_PanicOnCrash:InternalOnly bool true' \
  'Add :_PanicOnCrash:PanicOnConsecutiveCrash bool true'

# On macOS, we install the (disabled) customer ssh launchd plist
# as well as an Apple-internal enabled copy
xinstall $(src com.openssh.sshd.plist) ${system}/ssh.plist
xinstall ${sshplist} ${internal}/ssh.plist
