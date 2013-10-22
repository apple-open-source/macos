#!/bin/sh

# copy_postgresql_config_files.sh
# PostgreSQL

# Copyright (c) 2012 Apple Inc. All Rights Reserved.
#
# IMPORTANT NOTE: This file is licensed only for use on Apple-branded
# computers and is subject to the terms and conditions of the Apple Software
# License Agreement accompanying the package this file is a part of.
# You may not port this file to another platform without Apple's written consent.

#
# Copies the default PostgreSQL config file into /Library/Server during promotion or migration
# This file handles the copying for both server and customer instances of postgres.  Argument 1
#	should be passed in as either "server" or "customer" to specify which files to copy.
#

INSTANCE="$1"
ServerRoot="/Applications/Server.app/Contents/ServerRoot"
SourceConfig=""
DestConfig=""
ConfigDir=""
if [ "$INSTANCE" == "server" ]; then
	SourceConfig="$ServerRoot/Library/Preferences/com.apple.postgres.plist"
	ConfigDir="/Library/Server/PostgreSQL For Server Services/Config"
	DestConfig="$ConfigDir/com.apple.postgres.plist"
elif [ "$INSTANCE" == "customer" ]; then
	SourceConfig="$ServerRoot/Library/Preferences/org.postgresql.postgres.plist"
	ConfigDir="/Library/Server/PostgreSQL/Config"
	DestConfig="$ConfigDir/org.postgresql.postgres.plist"
else
	echo "No valid argument passed in; argument 1 should be either \"customer\" or \"server\""
	exit 1
fi

echo "Creating directory $ConfigDir..."
/bin/mkdir -p "$ConfigDir"
if [ $? != 0 ]; then
	echo "Error creating directory, exiting"
	exit 1;
fi
/usr/sbin/chown _postgres:_postgres "$ConfigDir"
/bin/chmod 0755 "$ConfigDir"

echo "Copying PostgreSQL $INSTANCE configuration files into /Libary/Server..."
/usr/bin/ditto "$SourceConfig" "$DestConfig"
/usr/sbin/chown _postgres:_postgres "$DestConfig"
/bin/chmod 644 "$DestConfig"
echo "Finished."
