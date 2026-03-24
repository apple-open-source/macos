#!/bin/sh

set -eu

MAGIC=/usr/share/file/magic
tstdir=/AppleInternal/Tests/file/file
utdir=/AppleInternal/CoreOS/BATS/unit_tests

cd "${SRCROOT}/file/tests"

tmplist=$(mktemp -t file_plist)
trap 'rm -f "$tmplist"' EXIT

cat <<EOF >>"${tmplist}"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>Project</key>
	<string>file</string>
	<key>IgnoreOutput</key>
	<true/>
	<key>Tests</key>
	<array>
		<dict>
			<key>TestName</key>
			<string>file.file_test.magic</string>
			<key>ShellEnv</key>
			<dict>
				<key>TZ</key>
				<string>UTC</string>
				<key>MAGIC</key>
				<string>${MAGIC}</string>
			</dict>
			<key>Command</key>
			<array>
				<string>./file_test</string>
			</array>
			<key>WorkingDirectory</key>
			<string>${tstdir}</string>
		</dict>
EOF

install -m 0755 -d "${DSTROOT}${tstdir}"

for testfile in *.testfile ; do
	testcase="${testfile%.testfile}"
	magic="${testcase}.magic"
	result="${testcase}.result"
	case "${testcase}" in
	*gpu*|*rdar*)
		hexdump -ve'1/1 "%02x"' "${testfile}" >"${DSTROOT}${tstdir}/${testfile}.hex"
		testfile="${testfile}.hex"
		chmod 0644 "${DSTROOT}${tstdir}/${testfile}"
		;;
	*)
		install -m 0644 "${testfile}" "${DSTROOT}${tstdir}/${testfile}"
		;;
	esac
	echo "installed ${DSTROOT}${tstdir}/${testfile}"
	sed -E 's@^../tests/@@' "${result}" >"${DSTROOT}${tstdir}/${result}"
	chmod 0644 "${DSTROOT}${tstdir}/${result}"
	echo "installed ${DSTROOT}${tstdir}/${result}"
	if [ -f "${magic}" ] ; then
		install -m 0644 "${magic}" "${DSTROOT}${tstdir}/${magic}"
		echo "installed ${DSTROOT}${tstdir}/${magic}"
	else
		magic="${MAGIC}"
	fi
	cat <<EOF >>"${tmplist}"
		<dict>
			<key>TestName</key>
			<string>file.file_test.${testcase}</string>
			<key>ShellEnv</key>
			<dict>
				<key>TZ</key>
				<string>UTC</string>
				<key>MAGIC</key>
				<string>${magic}</string>
			</dict>
			<key>Command</key>
			<array>
				<string>./file_test</string>
				<string>${testfile}</string>
				<string>${result}</string>
			</array>
			<key>WorkingDirectory</key>
			<string>${tstdir}</string>
		</dict>
EOF
done

cat <<EOF >>"${tmplist}"
	</array>
	<key>Timeout</key>
	<integer>30</integer>
</dict>
</plist>
EOF

plutil -lint "${tmplist}"
install -m 0755 -d "${DSTROOT}${utdir}"
install -m 0644 "${tmplist}" "${DSTROOT}${utdir}/file.plist"
echo "installed ${DSTROOT}${utdir}/file.plist"
