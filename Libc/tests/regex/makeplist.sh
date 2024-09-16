#!/bin/sh

cat <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>Tests</key>
	<array>
EOF

for datafile in datafiles/*.dat ; do
	name=$(basename ${datafile%%.dat})
	cat <<EOF
		<dict>
			<key>TestName</key>
			<string>Libc.regex.$name</string>
			<key>Command</key>
			<array>
				<string>/AppleInternal/Tests/Libc/testregex</string>
				<string>/AppleInternal/Tests/Libc/regex/$name.dat</string>
			</array>
			<key>MayRunConcurrently</key>
			<true/>
			<key>Timeout</key>
			<integer>300</integer>
		</dict>
EOF
done

cat <<EOF
	</array>
</dict>
</plist>
EOF
