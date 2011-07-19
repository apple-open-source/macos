#!/bin/sh

find . -name '*-Info.plist' -exec /usr/libexec/PlistBuddy -c "Set :CFBundleVersion $1" {} \;
find . -name '*-Info.plist' -exec /usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString $1" {} \;
