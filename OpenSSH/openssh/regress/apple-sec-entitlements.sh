# This test is meant to prevent security issues where third-party dylibs are
# loaded into code with privileged entitlements, such as for keychain access.

tid="Apple entitlements security"

clv="com.apple.private.security.clear-library-validation"

bins=""
bins+=" $TEST_SSH_SSH"
bins+=" $TEST_SSH_SSH-apple-pkcs11"
# sshd still needs clv in addition to other entitlements due to PAM
#bins+=" $TEST_SSH_SSHD"
bins+=" $TEST_SSH_SSHAGENT"
bins+=" $TEST_SSH_SSHADD"
bins+=" $TEST_SSH_SSHKEYGEN"
bins+=" $TEST_SSH_SSHKEYSCAN"
bins+=" $TEST_SSH_SFTP"
bins+=" $TEST_SSH_SFTPSERVER"
bins+=" $TEST_SSH_PKCS11_HELPER"

list_entitlements_for() {
	xml=`/usr/bin/codesign -d --xml --ent - "$1" 2>/dev/null`
	if [ -n "$xml" ]; then
		echo "$xml" | /usr/bin/xmllint - --xpath '/plist/dict/key/text()'
	fi
}

for bin in $bins; do
	trace "binary $bin"
	verbose "test $tid: binary $bin"
	entitlements=`list_entitlements_for "$bin"`
	if echo "$entitlements" | /usr/bin/grep -q "^$clv$"; then
		other_entitlements=`echo "$entitlements" | /usr/bin/grep -v "^$clv$"`
		for other_entitlement in $other_entitlements; do
			fail "$bin is entitled with both $clv and $other_entitlement"
		done
	fi
done
