global verticalPos
global needVerticalAdjust
global desiredInitialVerticalOffset

on makenewTab given theme:themeName
	tell application "Terminal"
		activate
		tell application "System Events" to keystroke "t" using command down
		repeat while contents of selected tab of front window starts with linefeed
			delay 0.01
		end repeat
		set current settings of selected tab of front window to first settings set whose name is themeName
	end tell
end makenewTab

on newPhoneTerm for phoneColor by phonePort
	set horizontalOffset to 50
	set height to 180
	set width to 1800
	tell application "Terminal"
		activate
		if phoneColor is equal to "Red" then
			set currentTheme to "Red Sands"
		else if phoneColor is equal to "Green" then
			set currentTheme to "Grass"
		else if phoneColor is equal to "Beige" then
			set currentTheme to "Novel"
		else if phoneColor is equal to "Black" then
			set currentTheme to "Pro"
		else if phoneColor is equal to "Cyan" then
			set currentTheme to "Cyan"
		else if phoneColor is equal to "Orange" then
			set currentTheme to "Orange"
		else if phoneColor is equal to "Beige" then
			set currentTheme to "Novel"
		else
			set currentTheme to "Ocean"
		end if
		
		-- make a new window with the execution of a trivial command
		do script "clear"
		
		-- load up the window id of the window we just created
		--	set window_id to id of first window whose frontmost is true
		set targetWindow to front window
		-- Put it on the right hand screen first
		set position of targetWindow to {horizontalOffset, 0}
		set position of targetWindow to {horizontalOffset, verticalPos}
		set size of targetWindow to {width, height}
		set position of targetWindow to {horizontalOffset, verticalPos}
		set pos to position of targetWindow
		if needVerticalAdjust and ((item 2 of pos) is not equal to verticalPos) then
			set needVerticalAdjust to false
			set verticalPos to (item 2 of pos) + desiredInitialVerticalOffset
			set position of targetWindow to {horizontalOffset, verticalPos}
		end if
		set verticalPos to verticalPos + height + 15
		
		set current settings of selected tab of targetWindow to first settings set whose name is currentTheme
		
		
		-- make tabs 2, 3, 4, 5
		repeat with i from 1 to 4
			makenewTab of me given theme:currentTheme
		end repeat
		
		-- for each of the five tabs we've now made
		repeat with i from 1 to 5
			
			-- build the command, then execute it
			if i is less than 5 then
				set myuser to "root"
			else
				set myuser to "mobile"
			end if
			if (i = 1) then
				set shcmd to "syslog -w"
			else if (i = 2) then
				set shcmd to "ls -1t /var/mobile/Library/Logs/CrashReporter/DiagnosticLogs/security.log.\\*Z \\| head -1 \\| xargs  tail -100000F"
			else if (i = 3) then
				set shcmd to "ls -1t /var/mobile/Library/Logs/CrashReporter/DiagnosticLogs/security.log.\\*Z \\| head -1 \\| xargs  tail -100000F \\| egrep \"'(event|keytrace|peer|coder|engine){}|<Error>'\""
			else if (i = 4) then
				set shcmd to "security item -q class=inet,sync=1 \\| grep acct \\| tail -3"
			else if (i = 5) then
				set shcmd to ""
				-- for c in inet genp keys; do for t in "" ,tomb=1; do security item class=$c,sync=1$t; done; done | grep agrp | wc -l
			else
				set shcmd to ""
			end if
			set custom title of tab i of targetWindow to phoneColor & " " & myuser & " " & phonePort
			set cmd to "~/bin/sshauser " & "--retry " & phonePort & " " & myuser & " " & shcmd
			do script cmd in tab i of targetWindow
			
		end repeat
		
	end tell
	
	
end newPhoneTerm

--
-- main code
--

set desiredInitialVerticalOffset to 10
set verticalPos to -10000
set needVerticalAdjust to true

newPhoneTerm of me for "Red" by 11022
newPhoneTerm of me for "Blue" by 12022
-- newPhoneTerm of me for "Green" by 12022
-- newPhoneTerm of me for "Cyan" by 13022
-- newPhoneTerm of me for "Orange" by 15022
-- newPhoneTerm of me for "Beige" by 16022
-- newPhoneTerm of me for "Black" by 21022

-- # Config file for ssh
-- #UDID 79003b34516ba80b620e3d947e7da96e033bed48 johnsrediphone 10022
-- #UDID 96476595e5d0ef7496e8ff852aedf4725647960b johnsblueiphone 11022
-- #UDID b674745cb6d2a1616a065cddae7207f91980e95d johnsgreentouch 12022
-- #UDID a489e67286bc2a509ef74cda67fc6696e2e1a192 johnscyanmini 13022
-- #UDID df86edbd280fd986f1cfae1517e65acbac7188cd johnsyellowmini 14022
-- #UDID 16d4c2e0a63083ec16e3f2ed4f21755b12deb900 johnsorangemini 15022
-- #UDID 8b2aa30e1ead1c7c303c363216bfe44f1cb21ce6 johnsbeigeipad 16022
-- #UDID f80b8fbf11ca6b8d692f10e9ea29dea1e57fcbdf johnswhiteipad 17022

