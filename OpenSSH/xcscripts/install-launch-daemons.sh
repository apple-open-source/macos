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
  'Delete :Disabled'

# On macOS, we install the (disabled) customer ssh launchd plist
# as well as an Apple-internal enabled copy
if [ "${PLATFORM_NAME}" = "macosx" ]; then
    xinstall $(src com.openssh.sshd.plist) ${system}/ssh.plist
    xinstall ${sshplist} ${internal}/ssh.plist
    exit 0
fi

# On embedded, there are three launchd plists, all Apple internal.
# First, the general use case.
pb ${sshplist} \
  'Set :Program /usr/local/libexec/sshd-keygen-wrapper' \
  'Add :Sockets:Listeners:SockNodeName string "localhost"' \
  'Delete :Sockets:Listeners:Bonjour' \
  'Add :Sockets:Listeners:Bonjour bool false' \
  'Delete :SHAuthorizationRight' \
  'Add :EnablePressuredExit bool false' \
  'Add :EnableTransactions bool false'
xinstall ${sshplist} ${internal}/com.openssh.sshd.plist
if [ -n "${RC_BRIDGE}" ]; then
    # One more adjustment for bridgeOS
    pb ${internal}/com.openssh.sshd.plist \
      'Delete :Sockets:Listeners:SockNodeName'
fi

# Second, an alternate port with password authentication disabled.
pb ${sshplist} \
  'Set :Label com.openssh.sshd2' \
  'Set :Sockets:Listeners:SockServiceName 2022' \
  'Add :ProgramArguments:1 string "-s"'
xinstall ${sshplist} ${internal}/com.openssh.sshd2.plist

# Third, yet another port, this time listening on all interfaces.
# This is only mastered for A144 and such.
pb ${sshplist} \
  'Set :Label com.openssh.sshd.sncp' \
  'Set :Sockets:Listeners:SockServiceName 10022' \
  'Delete :Sockets:Listeners:SockNodeName' \
  'Delete :ProgramArguments:1'
xinstall ${sshplist} ${internal}/com.openssh.sshd.sncp.plist

