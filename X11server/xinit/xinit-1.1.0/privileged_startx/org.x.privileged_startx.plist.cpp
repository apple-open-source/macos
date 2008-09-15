<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>Label</key>
		<string>org.x.privileged_startx</string>
	<key>MachServices</key>
		<dict>
			<key>org.x.privileged_startx</key>
				<true/>
		</dict>
	<key>ProgramArguments</key>
		<array>
			<string>XINITDIR/privileged_startx</string>
			<string>-d</string>
			<string>SCRIPTDIR</string>
		</array>
	<key>TimeOut</key>
		<integer>120</integer>
</dict>
</plist>
