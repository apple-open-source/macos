#!/bin/zsh
osascript -e 'Tell application "System Events" to display dialog "Enter password:" default answer "" with hidden answer' | awk -F': ' '/text returned:/ {print $2}'