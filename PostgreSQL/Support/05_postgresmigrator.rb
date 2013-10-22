#!/usr/bin/ruby
#
# 05_postgresmigrator.rb
#
# Migration script for PostgreSQL
# Supports migration from 10.7.x to the latest Server release
# When source system is 10.6.x, initializes an empty set of databases.
#
# Author:: Apple Inc.
# Documentation:: Apple Inc.
# Copyright (c) 2011-2013 Apple Inc. All Rights Reserved.
#
# IMPORTANT NOTE: This file is licensed only for use on Apple-branded
# computers and is subject to the terms and conditions of the Apple Software
# License Agreement accompanying the package this file is a part of.
# You may not port this file to another platform without Apple's written consent.
# License:: All rights reserved.
#
# This script upgrades 10.7.x (PostgreSQL 9.0) data to the latest server version (PostgreSQL 9.2).
# It also splits the original database into two new database clusters: One dedicated for customer use (in what was previously
# the default database location), and another dedicated for use only by the shipping server services.
#

require 'fileutils'
require 'logger'
require 'osx/cocoa'
include OSX
require 'socket'

$logFile = "/Library/Logs/ServerSetup.log"
$logger = Logger.new($logFile)
$logger.level = Logger::INFO
$logger.info("*** PostgreSQL migration start ***")

$serveradmin = "/Applications/Server.app/Contents/ServerRoot/usr/sbin/serveradmin"
$serverctl = "/Applications/Server.app/Contents/ServerRoot/usr/sbin/serverctl"
$postgresBinariesDir9_0 = "/Applications/Server.app/Contents/ServerRoot/usr/libexec/postgresql9.0"
$newPostgresBinariesDir = "/Applications/Server.app/Contents/ServerRoot/usr/bin"
$pgServiceDir = "/Library/Server/PostgreSQL For Server Services"
$pgServiceDirCustomer =  "/Library/Server/PostgreSQL"
$newPostgresDataDirServer = "/Library/Server/PostgreSQL For Server Services/Data"
$newPostgresDataDirCustomer = "/Library/Server/PostgreSQL/Data"
$customerSocketDir = "/private/var/pgsql_socket"
$pgLogDir = "/Library/Logs/PostgreSQL"
$serverSocketDir = "/Library/Server/PostgreSQL For Server Services/Socket"
$migrationDir = "/Library/Server/PostgreSQL For Server Services/Migration"
$serverDatabases =  ["caldav", "collab", "device_management"]	# databases to be forked away from customer data
$serverRoles = ["caldav", "collab", "_devicemgr"]				# roles to be forked away from customer data
$serverRolesSQL = "CREATE ROLE _devicemgr; ALTER ROLE _devicemgr WITH NOSUPERUSER INHERIT NOCREATEROLE CREATEDB LOGIN NOREPLICATION; CREATE ROLE caldav; ALTER ROLE caldav WITH NOSUPERUSER INHERIT NOCREATEROLE CREATEDB LOGIN NOREPLICATION; CREATE ROLE collab; ALTER ROLE collab WITH SUPERUSER INHERIT CREATEROLE CREATEDB LOGIN NOREPLICATION; CREATE ROLE webauth; ALTER ROLE webauth WITH SUPERUSER INHERIT CREATEROLE CREATEDB LOGIN NOREPLICATION;"

$purge = "0"
$sourceRoot = "/Previous System"
$targetRoot = "/"
$sourceType = "System"
$sourceVersion = "10.7"
$language = "en"
$orig_wd = Dir.getwd

def usage
	usage_str =<<EOS
usage: for example:\n
#{File.basename($0)} --sourceRoot "/Previous System" --targetRoot / --purge 0 --language en --sourceVersion 10.7 --sourceType System

In this implementation, --language and --sourceType are ignored
EOS
	$stderr.print(usage_str)
end

def exitWithError(message)
	$logger.error(message)
	$logger.info("*** PostgreSQL migration end ***")
	$logger.close
	Dir.chdir($orig_wd)
	exit(2)
end

def exitWithMessage(message)
	$logger.info(message)
	$logger.info("*** PostgreSQL migration end ***")
	$logger.close
	Dir.chdir($orig_wd)
	exit(0)
end

def runCommandOrExit(command)
	ret = `#{command}`
	if $? != 0
		$logger.warn("command failed: #$?\nCommand: #{command}\nOutput: #{ret}")
		exitWithError("Wiki and Profile Manager will not be available.")
	end
end

def runCommand(command)
	ret = `#{command}`
	if $? != 0
		$logger.warn("command failed: #$?\nCommand: #{command}\nOutput: #{ret}")
		return 1
	end
	return 0
end

def startNewPostgres
	runCommandOrExit("#{$serverctl} enable service=com.apple.postgres");
	isRunning = 0
	30.times do
		statusDict = dictionaryFromServerAdmin("fullstatus postgres_server")
		if statusDict["postgresIsResponding"]
			$logger.info("Confirmed that postgres is responding after upgrade/migration")
			isRunning = 1
			break
		end
		sleep 1
	end
	if (! isRunning)
		$logger.warn("Postgres is not responding after upgrade/migration: #{statusDict.inspect}")
		exitWithError("Wiki and Profile Manager will not be available.")
	end
end

def dictionaryFromServerAdmin(cmd)
	tempFilePath = "/tmp/#{File.basename($0)}-#{$$}"
	`/Applications/Server.app/Contents/ServerRoot/usr/sbin/serveradmin -x #{cmd} > #{tempFilePath}`
	exitWithError("serveradmin #{cmd} failed") if $?.exitstatus != 0
	dict = NSDictionary.dictionaryWithContentsOfFile(tempFilePath).to_ruby
	FileUtils.rm_f(tempFilePath)
	exitWithError("Could not obtain results from serveradmin #{cmd}") if dict.nil?
	return dict
end

def settingsFromSourceLaunchd
	settings = {}
	plist = "#{$sourceRoot}/System/Library/LaunchDaemons/org.postgresql.postgres.plist"
	
	exitWithError("Required file missing from previous system: #{plist}. This is probably an invalid attempt to upgrade/migrate from a non-server system") if !File.exists?(plist)
	dict = NSDictionary.dictionaryWithContentsOfFile(plist).to_ruby
	exitWithError("Could not obtain settings from source plist #{plist}") if dict.nil?
	args = dict["ProgramArguments"]
	exitWithError("Could not obtain ProgramArguments from source plist #{plist}") if args.nil?
	dataDirIndex = args.index("-D")
	exitWithError("Could not obtain dataDir from source plist #{plist}") if dataDirIndex.nil?
	dataDir = args[dataDirIndex + 1]
	exitWithError("Could not obtain dataDir from source plist #{plist}") if dataDir.nil?
	settings["dataDir"] = dataDir
	args.each do |arg|
		key,val = arg.split('=')
		settings[key] = val unless val.nil?
	end
	overridesPlist = "#{$sourceRoot}/private/var/db/launchd.db/com.apple.launchd/overrides.plist"
	overridesDict = NSDictionary.dictionaryWithContentsOfFile(overridesPlist).to_ruby
	exitWithError("Could not obtain settings from source overrides plist #{overridesPlist}") if overridesDict.nil?
	if overridesDict["org.postgresql.postgres"].nil?
		settings["state"] = dict["Disabled"] ? "STOPPED" : "RUNNING"
		else
		settings["state"] = overridesDict["org.postgresql.postgres"]["Disabled"] ? "STOPPED" : "RUNNING"
	end
	return settings
end

def shutDownOrphans
	# e. g. "20270 postgres: _devicemgr device_management [local] idle"
	procs = `ps -U _postgres -o pid,comm`
	return if procs.size == 0
	orphanLines = procs.scan(/.*idle$/)
	orphanLines.each do |line|
		pid = line[/^\d+/].to_i
		next if pid < 2
		sig = line.sub(/^\d+/, "")
		$logger.warn("Killing idle client process #{pid} with signature: #{sig}")
		Process.kill("TERM", pid)
	end
end

def megsAvailableOnVolume(volume)
	fileManager = NSFileManager.defaultManager
	path = NSString.alloc.initWithString(volume)
	dict = NSDictionary.dictionaryWithDictionary(fileManager.attributesOfFileSystemForPath_error_(path, nil))
	bytesAvailable = (dict["NSFileSystemFreeSize"].integerValue)
	megsAvailable = bytesAvailable / 1024 / 1024
	return megsAvailable
end

def forkDatabases(serverTargetDir)
	# Preflight and leave customer database cluster running in order to extract server-specific data
	# First disable any TCP sockets that are configured to prevent conflict with other postgres installations
	tempFilePath = "/tmp/#{File.basename($0)}-#{$$}"
	command = "echo \"postgres:listen_addresses=\\\"\\\"\" > #{tempFilePath}"
	runCommandOrExit(command)
	command = "#{$serveradmin} settings < #{tempFilePath}"
	runCommandOrExit(command)
	FileUtils.rm_f(tempFilePath)

	$logger.info("Restarting customer-specific postgres with new settings, to check for successful initialization")
	dictionaryFromServerAdmin("start postgres")
	statusDict = dictionaryFromServerAdmin("fullstatus postgres")
	if (! statusDict["postgresIsResponding"])
		$logger.warn("Customer postgres database cluster is not responding after upgrade/migration: #{statusDict.inspect}")
		exitWithError("Wiki and Profile Manager will not be available.")
	end

	$logger.info("Initializing the server-specific database cluster")
	command = "sudo -u _postgres #{$newPostgresBinariesDir}/initdb --encoding UTF8 --locale=C -D \"#{serverTargetDir}\""
	runCommandOrExit(command)

	$logger.info("Restarting server-specific postgres with new settings, to check for successful initialization")
	startNewPostgres

	$logger.info("Creating Server roles")
	command = "sudo -u _postgres #{$newPostgresBinariesDir}/psql postgres -h \"#{$serverSocketDir}\" -c \"#{$serverRolesSQL}\""
	runCommandOrExit(command)

	$logger.info("Moving Server databases to new database")
	command = "sudo -u _postgres #{$newPostgresBinariesDir}/createdb collab -O collab -h \"#{$serverSocketDir}\""
	runCommandOrExit(command)
	command = "sudo -u _postgres #{$newPostgresBinariesDir}/createdb caldav -O caldav -h \"#{$serverSocketDir}\""
	runCommandOrExit(command)
	command = "sudo -u _postgres #{$newPostgresBinariesDir}/createdb device_management -O _devicemgr -h \"#{$serverSocketDir}\""
	runCommandOrExit(command)
	for database in $serverDatabases
		command = "sudo -u _postgres #{$newPostgresBinariesDir}/pg_dump #{database} -h \"#{$customerSocketDir}\" | sudo -u _postgres #{$newPostgresBinariesDir}/psql -d #{database} -h \"#{$serverSocketDir}\""
		runCommand(command)
	end

	$logger.info("Dropping Server databases from customer database cluster")
	for database in $serverDatabases
		command = "sudo -u _postgres #{$newPostgresBinariesDir}/dropdb -h \"#{$customerSocketDir}\" #{database}"
		runCommand(command)
	end

	$logger.info("Dropping Server roles from customer database cluster")
	for role in $serverRoles
		command = "sudo -u _postgres #{$newPostgresBinariesDir}/dropuser -h \"#{$customerSocketDir}\" #{role}"
		runCommand(command)
	end
end

def initialize_for_clean_install
	pgExtrasDir = "/Applications/Server.app/Contents/ServerRoot/System/Library/ServerSetup/CommonExtras/PostgreSQLExtras"

	if File.exists?($newPostgresDataDirServer) || File.exists?($newPostgresDataDirCustomer)
		exitWithError("Data directory already exists where there should be no directory.  Exiting.")
	end
	
	$logger.info("Creating Data Directory for server database cluster")
	FileUtils.mkdir($newPostgresDataDirServer)
	FileUtils.chmod(0700, $newPostgresDataDirServer)
	FileUtils.chown("_postgres", "_postgres", $newPostgresDataDirServer)
	
	$logger.info("Calling initdb for server database cluster")
	command = "sudo -u _postgres #{$newPostgresBinariesDir}/initdb --encoding UTF8 -D \"#{$newPostgresDataDirServer}\""
	runCommandOrExit(command)

	if File.exists?(pgExtrasDir)
		$logger.info("Executing PostgreSQLExtras")
		d = Dir.new(pgExtrasDir)
		if d.entries.count > 2	# allow for ".." and "."
			startNewPostgres
			d.sort{|a,b| a.downcase <=> b.downcase}.each do |executable|
				next if executable == "." || executable == ".."
				command = "#{pgExtrasDir}/#{executable}"
				ret = runCommand(command)
				if (ret != 0)
					$logger.warn("Executable returned an error status: #{executable}")
				end
			end
			# Leave it running
		end
	end

	$logger.info("Creating Data Directory for customer database cluster")
	FileUtils.mkdir($newPostgresDataDirCustomer)
	FileUtils.chmod(0700, $newPostgresDataDirCustomer)
	FileUtils.chown("_postgres", "_postgres", $newPostgresDataDirCustomer)
	
	$logger.info("Calling initdb for customer database cluster")
	command = "sudo -u _postgres #{$newPostgresBinariesDir}/initdb --encoding UTF8 -D \"#{$newPostgresDataDirCustomer}\""
	runCommandOrExit(command)
end

######################################  MAIN
while arg = ARGV.shift
	case arg
		when /--purge/
		$purge = ARGV.shift
		when /--sourceRoot/
		$sourceRoot = ARGV.shift
		when /--targetRoot/
		$targetRoot = ARGV.shift
		when /--sourceType/
		$sourceType = ARGV.shift
		when /--sourceVersion/
		$sourceVersion = ARGV.shift
		when /--language/
		$language = ARGV.shift
		else
		$stderr.print "Invalid arg: " + arg + "\n"
		usage()
		Process.exit(1)
	end
end

$logger.info("#{$0} --purge " + $purge + " --sourceRoot " + $sourceRoot + " --targetRoot " + $targetRoot + " --sourceType " + $sourceType + " --sourceVersion " + $sourceVersion + " --language " + $language)
exitWithMessage("PostgreSQL migration from #{$sourceVersion} is not supported.") if ($sourceVersion !~ /10.7/ && $sourceVersion !~ /10.6/)
exitWithError("sourceRoot #{$sourceRoot} is not an existing directory") if !File.directory?($sourceRoot)
oldServerPlistFile = $sourceRoot + "/System/Library/CoreServices/ServerVersion.plist"
exitWithError("sourceRoot #{oldServerPlistFile} does not exist; this is an invalid attempt to upgrade/migrate from a non-server system") if !File.exists?(oldServerPlistFile)

if File.identical?($sourceRoot, $targetRoot)
	exitWithError("sourceRoot #{$sourceRoot} and targetRoot #{$targetRoot} are identical")
end

if !File.exists?($pgServiceDir)
	$logger.info("Creating Service Directory for server database")
	FileUtils.mkdir($pgServiceDir)
	FileUtils.chmod(0755, $pgServiceDir)
	FileUtils.chown("_postgres", "_postgres", $pgServiceDir)
end

if !File.exists?($pgServiceDirCustomer)
	$logger.info("Creating Service Directory for customer database")
	FileUtils.mkdir($pgServiceDirCustomer)
	FileUtils.chmod(0755, $pgServiceDirCustomer)
	FileUtils.chown("_postgres", "_postgres", $pgServiceDirCustomer)
end

if !File.exists?($serverSocketDir)
	puts "Creating Socket Directory"
	FileUtils.mkdir($serverSocketDir)
	FileUtils.chmod(0755, $serverSocketDir)
	FileUtils.chown("_postgres", "_postgres", $serverSocketDir)
end

if !File.exists?($customerSocketDir)
	puts "Creating Socket Directory for customer database cluster"
	FileUtils.mkdir($customerSocketDir)
	FileUtils.chmod(0755, $customerSocketDir)
	FileUtils.chown("_postgres", "_postgres", $customerSocketDir)
end

if !File.exists?($migrationDir)
	puts "Creating Migration Directory"
	FileUtils.mkdir($migrationDir)
	FileUtils.chmod(0700, $migrationDir)
	FileUtils.chown("_postgres", "_postgres", $migrationDir)
end

if !File.exists?($pgLogDir)
	puts "Creating Log Directory"
	FileUtils.mkdir($pgLogDir)
	FileUtils.chmod(0755, $pgLogDir)
	FileUtils.chown("_postgres", "_postgres", $pgLogDir)
end

command = "/Applications/Server.app/Contents/ServerRoot/usr/libexec/copy_postgresql_config_files.sh server"
runCommandOrExit(command)
command = "/Applications/Server.app/Contents/ServerRoot/usr/libexec/copy_postgresql_config_files.sh customer"
runCommandOrExit(command)

if ($sourceVersion =~ /10.6/)
	# Just initialize; nothing to migrate
	initialize_for_clean_install
	exitWithMessage("Finished initializing postgres")
end

settingsDict = settingsFromSourceLaunchd
oldDataDir = settingsDict["dataDir"]

Dir.chdir($migrationDir)

# Migration from PostgreSQL 9.0 to 9.2
$logger.info("Migrating data from an earlier PostgreSQL version")

statusDict = dictionaryFromServerAdmin("fullstatus postgres_server")
exitWithError("Could not obtain postgres state") if statusDict["state"].nil?
if statusDict["state"] == "RUNNING"
	newState = dictionaryFromServerAdmin("stop postgres_server")
	exitWithError("serveradmin stop failed") if $?.exitstatus != 0
	exitWithError("Cannot confirm that postgres has stopped: #{newState}") if newState["state"] != "STOPPED"
	$logger.info("Postgres is running; stopping to apply new settings")
else
	shutDownOrphans
end

statusDict = dictionaryFromServerAdmin("fullstatus postgres")
exitWithError("Could not obtain postgres state") if statusDict["state"].nil?
if statusDict["state"] == "RUNNING"
	newState = dictionaryFromServerAdmin("stop postgres")
	exitWithError("serveradmin stop failed") if $?.exitstatus != 0
	exitWithError("Cannot confirm that postgres has stopped: #{newState}") if newState["state"] != "STOPPED"
	$logger.info("Postgres is running; stopping to apply new settings")
else
	shutDownOrphans
end

if oldDataDir =~ /^\/Volumes\/.*/ && $targetRoot.eql?("/")
	# Source and destination are the same (alternate volume)
	exitWithError("dataDir not present at #{oldDataDir}") if !File.directory?(oldDataDir)
	exitWithError("dataDir missing PG_VERSION file; concluding it is not a PostgreSQL data directory at #{oldDataDir}") if !File.exists?(oldDataDir + "/PG_VERSION")
	exitWithError("dataDir missing configuration file; concluding it is not a PostgreSQL data directory at #{oldDataDir}") if !File.exists?(oldDataDir + "/postgresql.conf")

	customerTargetDir = oldDataDir
	oldDataDirString = NSString.alloc.initWithString(oldDataDir).stringByStandardizingPath
	oldDataDirComponents = NSArray.alloc.init
	oldDataDirComponents = oldDataDirString.componentsSeparatedByString("/")
	dbVolume = "/#{oldDataDirComponents[1]}/#{oldDataDirComponents[2]}"
	serverTargetDir = oldDataDirString.stringByDeletingLastPathComponent.stringByDeletingLastPathComponent + "/PostgreSQL For Server Services/Data"
	if (serverTargetDir.pathComponents.length < 4)
		exitWithError("Target path for destination server database is too short, exiting (path: #{serverTargetDir})")
	end

	# If enough space is available on the alternate volume, move the original database to a new
	#   directory on this volume and init a new database in place of the original.
	#   Otherwise, try using the targetRoot to store the original database.
	$logger.info("Moving out-of-date database aside to be upgraded")
	newSourceDataDir = nil
	megsAvailable = megsAvailableOnVolume(dbVolume)
	if (megsAvailable == 0)
		exitWithError("megsAvailable is 0 for volume #{dbVolume}")
	end
	dbSizeMegs = `du -m -s "#{customerTargetDir}" | awk '{print $1}'`.to_i
	if ((dbSizeMegs * 2 + 1024) < megsAvailable)  # enough space for a copy of the database onto the same source volume, plus a bit extra.
		newSourceDataDir = "#{dbVolume}/Library/Server/PostgreSQL/Data.before_PG9.1_upgrade"
		newSourceDataDirDir = File.dirname(newSourceDataDir)
		unless File.exists?(newSourceDataDirDir)
			FileUtils.mkdir_p(newSourceDataDirDir)
			FileUtils.chmod(0700, newSourceDataDirDir)
			FileUtils.chown("_postgres", "_postgres", newSourceDataDirDir)
		end
		FileUtils.mv(customerTargetDir, newSourceDataDir)
	else
		megsAvailable = megsAvailableOnVolume($targetRoot)
		if (megsAvailable == 0)
			exitWithError("megsAvailable is 0 for volume #{$targetRoot}")
		end
		if ((dbSizeMegs + 1024) < megsAvailable)  # enough space for a copy of the database, plus a bit extra.
			newSourceDataDir = "#{$targetRoot}/Library/Server/PostgreSQL/Data.before_PG9.1_upgrade"
			newSourceDataDirDir = File.dirname(newSourceDataDir)
			unless File.exists?(newSourceDataDirDir)
				FileUtils.chmod(0700, newSourceDataDirDir)
				FileUtils.chown("_postgres", "_postgres", newSourceDataDirDir)
			end
			FileUtils.cp_r(customerTargetDir, newSourceDataDir, :preserve => true)
			FileUtils.rm_rf(customerTargetDir)
		else
			exitWithError("Not enough space free on data volume to migrate PostgreSQL database.")
		end
	end

	unless File.exists?(customerTargetDir)
		FileUtils.mkdir_p(customerTargetDir)
		FileUtils.chmod(0700, customerTargetDir)
		FileUtils.chown("_postgres", "_postgres", customerTargetDir)
	end

	$logger.info("Initializing the customer-specific database cluster")
	command = "sudo -u _postgres #{$newPostgresBinariesDir}/initdb --encoding UTF8 --locale=C -D \"#{customerTargetDir}\""
	runCommandOrExit(command)

	# If the old data directory contains a .pid file due to Postgres not shutting down properly, get rid of the file so that we can attempt upgrade.
	# There should be no chance that a postmaster is actually using the old data directory at this point.
	if File.exists?(newSourceDataDir + "/postmaster.pid")
		$logger.info("There is a .pid file in the source data dir.  Removing it to attempt upgrade.")
		FileUtils.rm_f(newSourceDataDir + "/postmaster.pid")
	end

	$logger.info("Running pg_upgrade...")
	firstServer = TCPServer.new('127.0.0.1', 0)
	firstPort = firstServer.addr[1]
	secondServer = TCPServer.new('127.0.0.1', 0)
	secondPort = secondServer.addr[1]
	firstServer.close
	secondServer.close
	command = "sudo -u _postgres #{$newPostgresBinariesDir}/pg_upgrade -b #{$postgresBinariesDir9_0} -B #{$newPostgresBinariesDir} -d \"#{newSourceDataDir}\" -D \"#{customerTargetDir}\" -p #{firstPort} -P #{secondPort}"
	runCommandOrExit(command)

	unless File.exists?(serverTargetDir)
		FileUtils.mkdir_p(serverTargetDir)
		FileUtils.chmod(0700, serverTargetDir)
		FileUtils.chown("_postgres", "_postgres", serverTargetDir)
	end
	`/Applications/Server.app/Contents/ServerRoot/usr/sbin/serveradmin settings postgres_server:dataDir=\"#{serverTargetDir}\"`
	`/Applications/Server.app/Contents/ServerRoot/usr/sbin/serveradmin settings postgres:dataDir=\"#{customerTargetDir}\"`
	forkDatabases(serverTargetDir)

	if $purge == "1"
		if File.exists?("#{customerTargetDir}/PG_VERSION")
			$logger.warn("purging #{newSourceDataDir}")
			FileUtils.rm_rf(newSourceDataDir)
		else
			$logger.warn("Skipping purge because copy could not be confirmed")
		end
	end
else  #  Source and destination on the same volume
	sourceDir = NSString.alloc.initWithString($sourceRoot + oldDataDir).stringByStandardizingPath
	serverTargetDir = NSString.alloc.initWithString($targetRoot + $newPostgresDataDirServer).stringByStandardizingPath
	customerTargetDir = NSString.alloc.initWithString($targetRoot + $newPostgresDataDirCustomer).stringByStandardizingPath

	exitWithError("dataDir not present at #{sourceDir}") if !File.directory?(sourceDir)
	exitWithError("dataDir missing PG_VERSION file; concluding it is not a PostgreSQL data directory at #{sourceDir}") if !File.exists?(sourceDir + "/PG_VERSION")
	exitWithError("dataDir missing configuration file; concluding it is not a PostgreSQL data directory at #{sourceDir}") if !File.exists?(sourceDir + "/postgresql.conf")

	if  File.exists?("#{serverTargetDir}/PG_VERSION")
		timestamp = Time.now.strftime("%Y-%m-%d %H:%M")
		backupDir = "#{serverTargetDir}.#{timestamp}"
		$logger.info("There appears to be an existing database in the target location.  Moving it aside to : #{backupDir}")
		FileUtils.mv("#{serverTargetDir}", "#{backupDir}")
	end

	if  File.exists?("#{customerTargetDir}/PG_VERSION")
		timestamp = Time.now.strftime("%Y-%m-%d %H:%M")
		backupDir = "#{customerTargetDir}.#{timestamp}"
		$logger.info("There appears to be an existing database in the target location.  Moving it aside to : #{backupDir}")
		FileUtils.mv("#{customerTargetDir}", "#{backupDir}")
	end

	unless File.exists?(serverTargetDir)
		FileUtils.mkdir_p(serverTargetDir)
		FileUtils.chmod(0700, serverTargetDir)
		FileUtils.chown("_postgres", "_postgres", serverTargetDir)
	end

	unless File.exists?(customerTargetDir)
		FileUtils.mkdir_p(customerTargetDir)
		FileUtils.chmod(0700, customerTargetDir)
		FileUtils.chown("_postgres", "_postgres", customerTargetDir)
	end

	# If the old data directory contains a .pid file due to Postgres not shutting down properly, get rid of the file so that we can attempt upgrade.
	# There should be no chance that a postmaster is actually using the old data directory at this point.
	if File.exists?(sourceDir +  "/postmaster.pid")
		$logger.info("There is a .pid file in the source data dir.  Removing it to attempt upgrade.")
		FileUtils.rm_f(sourceDir + "/postmaster.pid")
	end

	# Because we are forking server-specific items into their own database cluster, we first
	# pg_upgrade everything into the customer database cluster, then extract the server-specific items and
	# move them into the server-specific cluster.

	$logger.info("Initializing the server-specific database cluster")
	command = "sudo -u _postgres #{$newPostgresBinariesDir}/initdb --encoding UTF8 --locale=C -D \"#{customerTargetDir}\""
	runCommandOrExit(command)

	$logger.info("Running pg_upgrade...")
	firstServer = TCPServer.new('127.0.0.1', 0)
	firstPort = firstServer.addr[1]
	secondServer = TCPServer.new('127.0.0.1', 0)
	secondPort = secondServer.addr[1]
	firstServer.close
	secondServer.close
	command = "sudo -u _postgres #{$newPostgresBinariesDir}/pg_upgrade -b #{$postgresBinariesDir9_0} -B #{$newPostgresBinariesDir} -d \"#{sourceDir}\" -D \"#{customerTargetDir}\" -p #{firstPort} -P #{secondPort}"
	runCommandOrExit(command)

	`/Applications/Server.app/Contents/ServerRoot/usr/sbin/serveradmin settings postgres_server:dataDir=\"#{serverTargetDir}\"`
	`/Applications/Server.app/Contents/ServerRoot/usr/sbin/serveradmin settings postgres:dataDir=\"#{customerTargetDir}\"`
	forkDatabases(serverTargetDir)

	if $purge == "1"
		if File.exists?("#{customerTargetDir}/PG_VERSION") && File.exists?("#{serverTargetDir}/PG_VERSION")
			$logger.warn("purging #{sourceDir}")
			FileUtils.rm_rf(sourceDir)
			else
			$logger.warn("Skipping purge because copy could not be confirmed")
		end
	elsif File.exists?("#{sourceDir}/global/pg_control.old")
		FileUtils.mv("#{sourceDir}/global/pg_control.old", "#{sourceDir}/global/pg_control")
	end
end

$logger.info("Stopping the customer instance of PostgreSQL")
dictionaryFromServerAdmin("stop postgres")

$logger.info("Restoring default listen_addresses setting for customer instance of PostgreSQL")
command = "#{$serveradmin} settings postgres:listen_addresses=\"127.0.0.1,::1\""
runCommandOrExit(command)

# Leave the new postgres instance running.  Services should not have to start it.

statusDict = dictionaryFromServerAdmin("fullstatus postgres_server")
$logger.info("Current postgres_server fullstatus: #{statusDict.inspect}")
statusDict = dictionaryFromServerAdmin("fullstatus postgres")
$logger.info("Current postgres fullstatus: #{statusDict.inspect}")
exitWithMessage("Finished migrating data from an earlier PostgreSQL version.")
