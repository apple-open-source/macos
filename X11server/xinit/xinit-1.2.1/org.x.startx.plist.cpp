<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>Label</key>
		<string>LAUNCHD_ID_PREFIX.startx</string>
	<key>ProgramArguments</key>
		<array>
			<string>__bindir__/startx</string>
		</array>
	<key>Sockets</key>
		<dict>
			<key>LAUNCHD_ID_PREFIX:0</key>
				<dict>
					<key>SecureSocketWithKey</key>
						<string>DISPLAY</string>
				</dict>
		</dict>
	<key>ServiceIPC</key>
		<true/>
	<key>EnableTransactions</key>
		<true/>
</dict>
</plist>
