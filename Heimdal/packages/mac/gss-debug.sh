#!/bin/sh

echo ""
echo "- Install profile in System Preferences when prompted"
echo "- Quit System Presences when you finished installing the profile"

open -W "/System/Library/Frameworks/GSS.framework/Resources/GSS Debug Logging (OS X).configprofile"

killall -m gssd NetAuth

echo ""
echo "- Reproduce the issue, process control-c (^C) and then attach output to the bug report"
echo "- When done you can delete the profile in System Preferences"
echo ""

syslog -w
