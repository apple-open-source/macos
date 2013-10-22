#!/usr/bin/ruby
#
# 58_postgres_server_setup.rb
#
# Author:: Apple Inc.
# Documentation:: Apple Inc.
# Copyright (c) 2012-2013 Apple Inc. All Rights Reserved.
#
# IMPORTANT NOTE: This file is licensed only for use on Apple-branded
# computers and is subject to the terms and conditions of the Apple Software
# License Agreement accompanying the package this file is a part of.
# You may not port this file to another platform without Apple's written consent.
# License:: All rights reserved.
#
# PromotionExtra for PostgreSQL.
# This handles all promotion cases for the both the customer and server-services instances of postgres.
# This includes initialization of postgres database and dependencies, execution of PostgreSQLExtras for
# other services, and migration of postgres files when appropriate.
#
# This script should execute before any scripts for dependent services.
#

require 'fileutils'
require 'logger'
require 'osx/cocoa'
include OSX
require 'socket'

$logFile = "/Library/Logs/ServerSetup.log"
$logger = Logger.new($logFile)
$logger.level = Logger::INFO

$myPostgresVersion = "9.2"

#binaries
$newPostgresBinariesDir = "/Applications/Server.app/Contents/ServerRoot/usr/bin"
$psql = "#{$newPostgresBinariesDir}/psql"
$initdb = "#{$newPostgresBinariesDir}/initdb"
$pg_upgrade = "#{$newPostgresBinariesDir}/pg_upgrade"
$createdb = "#{$newPostgresBinariesDir}/createdb"
$dropuser = "#{$newPostgresBinariesDir}/dropuser"
$dropdb = "#{$newPostgresBinariesDir}/dropdb"
$pg_dump = "#{$newPostgresBinariesDir}/pg_dump"
$serveradmin = "/Applications/Server.app/Contents/ServerRoot/usr/sbin/serveradmin"
$serverctl = "/Applications/Server.app/Contents/ServerRoot/usr/sbin/serverctl"

#globals
$pgExtrasDir = "/Applications/Server.app/Contents/ServerRoot/System/Library/ServerSetup/CommonExtras/PostgreSQLExtras"
$pgLogDir = "/Library/Logs/PostgreSQL"
$pgServiceDir = "/Library/Server/PostgreSQL For Server Services"
$pgDataDir = "#{$pgServiceDir}/Data"
$pgSocketDir = "#{$pgServiceDir}/Socket"
$pgConfigDir = "#{$pgServiceDir}/Config"
$pgConfigFile = "#{$pgServiceDir}/Config/com.apple.postgres.plist"
$pgServiceDirCustomer = "/Library/Server/PostgreSQL"
$pgDataDirCustomer = "#{$pgServiceDirCustomer}/Data"
$pgSocketDirCustomer = "/private/var/pgsql_socket"
$pgConfigFileCustomer = "#{$pgServiceDirCustomer}/Config/org.postgresql.postgres.plist"

# variables for data upgrades from Server 2.1
$postgresBinariesDir9_1 = "/Applications/Server.app/Contents/ServerRoot/usr/libexec/postgresql9.1"
$migrationDir = "/Library/Server/PostgreSQL For Server Services/Migration"
$migrationDirCustomer = "/Library/Server/PostgreSQL/Migration"
$serverDatabases =  ["caldav", "collab", "device_management"]	# databases to be forked away from customer data
$serverRoles = ["caldav", "collab", "_devicemgr", "webauth"]				# roles to be forked away from customer data
$serverRolesSQL = "CREATE ROLE _devicemgr; ALTER ROLE _devicemgr WITH NOSUPERUSER INHERIT NOCREATEROLE CREATEDB LOGIN NOREPLICATION; CREATE ROLE caldav; ALTER ROLE caldav WITH NOSUPERUSER INHERIT NOCREATEROLE CREATEDB LOGIN NOREPLICATION; CREATE ROLE collab; ALTER ROLE collab WITH SUPERUSER INHERIT CREATEROLE CREATEDB LOGIN NOREPLICATION; CREATE ROLE webauth; ALTER ROLE webauth WITH SUPERUSER INHERIT CREATEROLE CREATEDB LOGIN NOREPLICATION;"

def exitWithError(message)
	$logger.error(message)
	$logger.info("*** PostgreSQL Promotion end ***")
	$logger.close
	exit(2)
end

def exitWithMessage(message)
	$logger.info(message)
	$logger.info("*** PostgreSQL Promotion end ***")
	$logger.close
	exit(0)
end

def runCommand(command)
	ret = `#{command}`
	if $? != 0
		$logger.warn("command failed: #$?\nCommand: #{command}\nOutput: #{ret}")
		return 1
	end
	return 0
end

def runCommandOrExit(command)
	ret = `#{command}`
	if $? != 0
		$logger.warn("command failed: #$?\nCommand: #{command}\nOutput: #{ret}")
		exitWithError("Wiki and Profile Manager will not be available.")
	end
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
	command = "#{$serveradmin} -x #{cmd} > #{tempFilePath}"
	runCommandOrExit(command)
	dict = NSDictionary.dictionaryWithContentsOfFile(tempFilePath).to_ruby
	FileUtils.rm_f(tempFilePath)
	exitWithError("Could not obtain results from serveradmin #{cmd}") if dict.nil?
	return dict
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
	command = "sudo -u _postgres #{$initdb} --encoding UTF8 --locale=C -D \"#{serverTargetDir}\""
	runCommandOrExit(command)

	startNewPostgres

	$logger.info("Creating Server roles")
	command = "sudo -u _postgres #{$psql} postgres -h \"#{$pgSocketDir}\" -c \"#{$serverRolesSQL}\""
	runCommandOrExit(command)

	$logger.info("Moving Server databases to new database")
	command = "sudo -u _postgres #{$createdb} collab -O collab -h \"#{$pgSocketDir}\""
	runCommandOrExit(command)
	command = "sudo -u _postgres #{$createdb} caldav -O caldav -h \"#{$pgSocketDir}\""
	runCommandOrExit(command)
	command = "sudo -u _postgres #{$createdb} device_management -O _devicemgr -h \"#{$pgSocketDir}\""
	runCommandOrExit(command)
	for database in $serverDatabases
		command = "sudo -u _postgres #{$pg_dump} #{database} -h \"#{$pgSocketDirCustomer}\" | sudo -u _postgres #{$psql} -d #{database} -h \"#{$pgSocketDir}\""
		runCommand(command)
	end

	# 'webauth' db is not migrated to the new database, but we need to drop it if it exists
	command = "sudo -u _postgres #{$dropdb} -h \"#{$pgSocketDirCustomer}\" webauth"
	runCommand(command)

	$logger.info("Dropping Server databases from customer database cluster")
	for database in $serverDatabases
		command = "sudo -u _postgres #{$dropdb} -h \"#{$pgSocketDirCustomer}\" #{database}"
		runCommand(command)
	end

	$logger.info("Dropping Server roles from customer database cluster")
	for role in $serverRoles
		command = "sudo -u _postgres #{$dropuser} -h \"#{$pgSocketDirCustomer}\" #{role}"
		runCommand(command)
	end
end

########################### MAIN #########################

$logger.info("*** PostgreSQL Promotion start ***")

serviceDirAlreadyExists = true

# Make sure that the requirements for postgres are met
if !File.exists?($pgServiceDir)
	serviceDirAlreadyExists = false
	$logger.info("Creating Service Directory for server database")
	FileUtils.mkdir($pgServiceDir)
	FileUtils.chmod(0755, $pgServiceDir)
	FileUtils.chown("_postgres", "_postgres", $pgServiceDir)
end

if !File.exists?($pgSocketDir)
	$logger.info("Creating Socket Directory for server database")
	FileUtils.mkdir($pgSocketDir)
	FileUtils.chmod(0755, $pgSocketDir)
	FileUtils.chown("_postgres", "_postgres", $pgSocketDir)
end

if !File.exists?($pgServiceDirCustomer)
	$logger.info("Creating Service Directory for customer database")
	FileUtils.mkdir($pgServiceDirCustomer)
	FileUtils.chmod(0755, $pgServiceDirCustomer)
	FileUtils.chown("_postgres", "_postgres", $pgServiceDirCustomer)
end

if !File.exists?($pgSocketDirCustomer)
	$logger.info("Creating Socket Directory for customer database")
	FileUtils.mkdir($pgSocketDirCustomer)
	FileUtils.chmod(0755, $pgSocketDirCustomer)
	FileUtils.chown("_postgres", "_postgres", $pgSocketDirCustomer)
end

if !File.exists?($pgLogDir)
	$logger.info("Creating Log Directory")
	FileUtils.mkdir($pgLogDir)
	FileUtils.chmod(0755, $pgLogDir)
	FileUtils.chown("_postgres", "_postgres", $pgLogDir)
end

## Config file migration/creation (only the data location is migrated)
# If upgrading from Server 2.1, copy the configuration (which may specify an alternate data location) to the new 'server' config location
if File.exists?($pgConfigFileCustomer) && !File.exists?($pgConfigFile)
	if !File.exists?($pgConfigDir)
		$logger.info("Creating Config Directory for server database cluster")
		FileUtils.mkdir($pgConfigDir)
		FileUtils.chmod(0755, $pgConfigDir)
		FileUtils.chown("_postgres", "_postgres", $pgConfigDir)
	end

	$logger.info("Copying template postgres config files for server database cluster for data location key migration")
	command = "/Applications/Server.app/Contents/ServerRoot/usr/libexec/copy_postgresql_config_files.sh server"
	runCommandOrExit(command)
	content = File.open($pgConfigFileCustomer) do |file|
		catch :lastline do
			file.each_line {|line|
				use_next = 0
				if line =~ /<string>-D<\/string>/
					use_next = 1
				elsif use_next && line =~ /<string>(.*)<\/string>/
					if ($1 != $pgDataDirCustomer)  # if source system did not use the default data location
						$pgDataDir = $1
						$logger.info("Upgrade: Found dataDir value in #{$pgConfigFileCustomer}, using it: #{$pgDataDir}")
					end
					throw :lastline
				end
			}
		end
	end
	FileUtils.rm_f($pgConfigFileCustomer)	# The data location in the file is no longer accurate, so delete the file

elsif File.exists?($pgConfigFile)
	# Get the data directory from config file it if exists
	content = File.open($pgConfigFile) do |file|
        catch :lastline do
			file.each_line {|line|
                use_next = 0
                if line =~ /<string>-D<\/string>/
					use_next = 1
				elsif use_next && line =~ /<string>(.*)<\/string>/
					$pgDataDir = $1
					$logger.info("Found dataDir value in #{$pgConfigFile}, using it: #{$pgDataDir}")
					throw :lastline
                end
			}
		end
	end
else
	$logger.info("Copying template postgres config files for server database cluster")
	command = "/Applications/Server.app/Contents/ServerRoot/usr/libexec/copy_postgresql_config_files.sh server"
	runCommandOrExit(command)
end

if File.exists?($pgConfigFileCustomer)
	content = File.open($pgConfigFileCustomer) do |file|
        catch :lastline do
			file.each_line {|line|
                use_next = 0
                if line =~ /<string>-D<\/string>/
					use_next = 1
				elsif use_next && line =~ /<string>(.*)<\/string>/
					$pgDataDirCustomer = $1
					$logger.info("Found dataDir value in #{$pgConfigFileCustomer}, using it: #{$pgDataDirCustomer}")
					throw :lastline
                end
			}
		end
	end
else
	$logger.info("Copying template postgres config files for customer database cluster")
	command = "/Applications/Server.app/Contents/ServerRoot/usr/libexec/copy_postgresql_config_files.sh customer"
	runCommandOrExit(command)
end

## Data initialization / migration
if !File.exists?($pgDataDir) && !File.exists?($pgDataDirCustomer)
	# Clean install
	$logger.info("Creating Data Directory for server database cluster")
	FileUtils.mkdir($pgDataDir)
	FileUtils.chmod(0700, $pgDataDir)
	FileUtils.chown("_postgres", "_postgres", $pgDataDir)

	$logger.info("Calling initdb for server database cluster")
	command = "sudo -u _postgres #{$initdb} --encoding UTF8 -D \"#{$pgDataDir}\""
	runCommandOrExit(command)

	startNewPostgres

	if File.exists?($pgExtrasDir)
		$logger.info("Executing PostgreSQLExtras")
		d = Dir.new($pgExtrasDir)
		if d.entries.count > 2	# allow for ".." and "."
			d.sort{|a,b| a.downcase <=> b.downcase}.each do |executable|
				next if executable == "." || executable == ".."
				command = "#{$pgExtrasDir}/#{executable}"
				ret = runCommand(command)
				if (ret != 0)
					$logger.warn("Executable returned an error status: #{executable}")
				end
			end
		end
	end

	$logger.info("Creating Data Directory for customer database cluster")
	FileUtils.mkdir($pgDataDirCustomer)
	FileUtils.chmod(0700, $pgDataDirCustomer)
	FileUtils.chown("_postgres", "_postgres", $pgDataDirCustomer)

	$logger.info("Calling initdb for customer database cluster")
	command = "sudo -u _postgres #{$initdb} --encoding UTF8 -D \"#{$pgDataDirCustomer}\""
	runCommandOrExit(command)
else
	# If the existing database is outdated, run pg_upgrade.  Otherwise we have an unexpected or unsupported case.
	if File.exists?("#{$pgDataDir}/PG_VERSION")
		pgVersion = File.open("#{$pgDataDir}/PG_VERSION", "rb"){ |f| f.read }.chomp
	elsif File.exists?("#{$pgDataDirCustomer}/PG_VERSION")
		pgVersion = File.open("#{$pgDataDirCustomer}/PG_VERSION", "rb"){ |f| f.read }.chomp
	else
		$logger.error("Could not find a valid postgres data directory at #{$pgDataDir} or #{$pgDataDirCustomer}")
		exitWithError("Wiki and Profile Manager will not be available.")
	end
	if serviceDirAlreadyExists && File.exists?($pgDataDir)
		if (pgVersion == "9.1" || pgVersion == "9.0")
			exitWithError("Source has a pre-2.2 version but a 2.2 data directory path.  Exiting.")
		end

		d = Dir.new($pgDataDir)
		if d.entries.count > 2
			# We already have a 9.2 data directory, so this is likely either a repromotion, or an install over some old data.
			# For repromotion cases, keep the existing directory.  For other cases, move aside the database and treat as a fresh install.
			# Make this decision based on whether the "customer" database cluster exists, as it currently does not relocate.
			if File.exists?($pgDataDirCustomer)
				startNewPostgres
				exitWithMessage("Both data directories aleady exist, probably a repromotion.  Leaving them alone.")
			else
				timestamp = Time.now.strftime("%Y-%m-%d %H:%M")
				FileUtils.mv($pgDataDir, "#{$pgDataDir}.found_during_server_install.#{timestamp}")
				$logger.info("Creating Data Directory for server database cluster")
				FileUtils.mkdir($pgDataDir)
				FileUtils.chmod(0700, $pgDataDir)
				FileUtils.chown("_postgres", "_postgres", $pgDataDir)

				$logger.info("Calling initdb for server database cluster")
				command = "sudo -u _postgres #{$initdb} --encoding UTF8 -D \"#{$pgDataDir}\""
				runCommandOrExit(command)

				startNewPostgres

				if File.exists?($pgExtrasDir)
					$logger.info("Executing PostgreSQLExtras")
					d = Dir.new($pgExtrasDir)
					if d.entries.count > 2	# allow for ".." and "."
						d.sort{|a,b| a.downcase <=> b.downcase}.each do |executable|
							next if executable == "." || executable == ".."
							command = "#{$pgExtrasDir}/#{executable}"
							ret = runCommand(command)
							if (ret != 0)
								$logger.warn("Executable returned an error status: #{executable}")
							end
						end
					end
				end
				
				$logger.info("Creating Data Directory for customer database cluster")
				FileUtils.mkdir($pgDataDirCustomer)
				FileUtils.chmod(0700, $pgDataDirCustomer)
				FileUtils.chown("_postgres", "_postgres", $pgDataDirCustomer)
				
				$logger.info("Calling initdb for customer database cluster")
				command = "sudo -u _postgres #{$initdb} --encoding UTF8 -D \"#{$pgDataDirCustomer}\""
				runCommandOrExit(command)
				exitWithMessage("Found existing data directory and moved it aside to initialize new cluster.")
			end
		else
			# If a previous upgrade to 2.2 failed due to a known port conflict issue with 3rd party installations, we can salvage
			# the data from the failed migration attempt.

			# Start customer postgres instance without using a TCP port to prevent any conflict, then see if it contains databases that need to be forked out
			tempFilePath = "/tmp/#{File.basename($0)}-#{$$}"
			command = "echo \"postgres:listen_addresses=\\\"\\\"\" > #{tempFilePath}"
			runCommandOrExit(command)
			command = "#{$serveradmin} settings < #{tempFilePath}"
			runCommandOrExit(command)
			FileUtils.rm_f(tempFilePath)
			dictionaryFromServerAdmin("start postgres")
			statusDict = dictionaryFromServerAdmin("fullstatus postgres")
			if (! statusDict["postgresIsResponding"])
				$logger.warn("Customer postgres database cluster is not responding after attempting to inspect it for a failed migration: #{statusDict.inspect}")
				exitWithError("Wiki and Profile Manager will not be available.")
			end
			command = "sudo -u _postgres #{$psql} postgres -h \"#{$pgSocketDirCustomer}\" -c \'\\l\'"
			ret = `#{command}`
			if $? != 0
				$logger.warn("command failed: #$?\nCommand: #{command}\nOutput: #{ret}")
				exitWithError("Wiki and Profile Manager will not be available.")
			end
			found_databases = 0
			ret.each_line {|line|
				if line =~ /^(\s*)(\S+)/
					if ($serverDatabases.include? $2)
						found_databases += 1
						$logger.info("Found database in customer database cluster that should be forked to new cluster: #{$2}")
					end
				end
			}
			if (found_databases == $serverDatabases.length)
				# We need to move aside any existing directory at the target server-services location in order for forkDatabases to do its thing
				FileUtils.mv($pgDataDir, "#{$pgDataDir}.presumed_bad_and_to_be_replaced")
				FileUtils.mkdir_p($pgDataDir)
				FileUtils.chmod(0700, $pgDataDir)
				FileUtils.chown("_postgres", "_postgres", $pgDataDir)
				dictionaryFromServerAdmin("stop postgres")
				forkDatabases($pgDataDir)
				command = "#{$serveradmin} settings postgres_server:dataDir=\"#{$pgDataDir}\""
				runCommandOrExit(command)
				command = "#{$serveradmin} settings postgres:dataDir=\"#{$pgDataDirCustomer}\""
				runCommandOrExit(command)

				$logger.info("Stopping the customer instance of PostgreSQL")
				dictionaryFromServerAdmin("stop postgres")

				$logger.info("Restoring default listen_addresses setting for customer instance of PostgreSQL")
				command = "#{$serveradmin} settings postgres:listen_addresses=\"127.0.0.1,::1\""
				runCommandOrExit(command)

				# Leave the new postgres instance running.  Services should not have to start it.
				exitWithMessage("Finished.  Repaired what appeared to be a broken Server 2.2 database cluster.")
			else
				startNewPostgres
				exitWithMessage("A database already exists at #{$pgDataDir}, there should be nothing left to do.")
			end
		end
	elsif (pgVersion != "9.1")
		# Upgrades only supported from 9.1 here; upgrades from 9.0 are handled in MigrationExtras script
		exitWithError("We want to upgrade database but its PG_VERSION specifies an unsupported version (#{pgVersion})")
	end

	unless File.exists?($migrationDir)
		FileUtils.mkdir_p($migrationDir)
		FileUtils.chmod(0700, $migrationDir)
		FileUtils.chown("_postgres", "_postgres", $migrationDir)
	end

	# Coming from Server 2.1, what is now the 'customer' data directory contains all of our source data to be upgraded and forked.
	# There are two scenarios depending on whether the data location was set to an alternate location in the previous settings.
	if $pgDataDir =~ /^\/Volumes\/.*/ && File.exists?($pgDataDir)
		# Handle the case where the database resides in a non-default location
		exitWithError("dataDir missing configuration file; concluding it is not a PostgreSQL data directory at #{$pgDataDir}") if !File.exists?($pgDataDir + "/postgresql.conf")

		timestamp = Time.now.strftime("%Y-%m-%d %H:%M")
		newSourceDataDir = "#{$pgDataDir}.#{timestamp}.before_upgrade_to_postgres_#{$myPostgresVersion}"
		$logger.info("Moving previous database directory aside to : #{newSourceDataDir}")
		FileUtils.mv("#{$pgDataDir}", "#{newSourceDataDir}")

		# Use the alternate volume as the initial target for both database clusters so that we aren't
		# moving server services data to the boot volume and then back to the alternate volume.
		# As of Server 2.2, the customer database cluster will always reside on the boot volume unless
		# it is manually relocated by the customer.
		tempCustomerDataDir = "#{$pgDataDir}.customerTemp"
		FileUtils.mkdir_p(tempCustomerDataDir)
		FileUtils.chmod(0700, tempCustomerDataDir)
		FileUtils.chown("_postgres", "_postgres", tempCustomerDataDir)
		$logger.info("Initializing new database cluster for migration target")
		command = "sudo -u _postgres #{$initdb} --encoding UTF8 --locale=C -D \"#{tempCustomerDataDir}\""
		runCommandOrExit(command)

		# If the old data directory contains a .pid file due to Postgres not shutting down properly, get rid of the file so that we can attempt upgrade.
		# There should be no chance that a postmaster is actually using the old data directory at this point.
		if File.exists?(newSourceDataDir +  "/postmaster.pid")
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
		origWorkingDirectory = Dir.getwd
		Dir.chdir($migrationDir)
		command = "sudo -u _postgres #{$pg_upgrade} -b #{$postgresBinariesDir9_1} -B #{$newPostgresBinariesDir} -d \"#{newSourceDataDir}\" -D \"#{tempCustomerDataDir}\" -p #{firstPort} -P #{secondPort}"
		ret = runCommand(command)
		if (ret != 0)
			Dir.chdir(origWorkingDirectory)
			exitWithError("Wiki and Profile Manager will not be available..")
		end
		Dir.chdir(origWorkingDirectory)

		# Temporarily update the settings to point to the temporary cluster location so that forkDatabases uses the correct source
		command = "#{$serveradmin} settings postgres:dataDir=\"#{tempCustomerDataDir}\""
		runCommandOrExit(command)

		pathComponents = $pgDataDir.split(File::SEPARATOR)
		$pgDataDir = "/#{pathComponents[1]}/#{pathComponents[2]}#{$pgServiceDir}/Data"

		FileUtils.mkdir_p($pgDataDir)
		FileUtils.chmod(0700, $pgDataDir)
		FileUtils.chown("_postgres", "_postgres", $pgDataDir)

		command = "#{$serveradmin} settings postgres_server:dataDir=\"#{$pgDataDir}\""
		runCommandOrExit(command)

		forkDatabases($pgDataDir)

		if File.exists?($pgDataDirCustomer)
			backupCustomerDataDir = "#{$pgDataDirCustomer}.#{timestamp}.previous"
			FileUtils.mv($pgDataDirCustomer, backupCustomerDataDir)
		end

		FileUtils.mv(tempCustomerDataDir, $pgDataDirCustomer)
	
	else
		# Default case: Data resides in what is now the 'customer' database cluster and it needs to be migrated to the new 'server' cluster
		exitWithError("dataDir missing configuration file; concluding it is not a PostgreSQL data directory at #{$pgDataDirCustomer}") if !File.exists?($pgDataDirCustomer + "/postgresql.conf")

		# Because we are forking server-specific items into their own database cluster, we first
		# pg_upgrade everything into the customer database cluster, then extract the server-specific items and
		# move them into the server-specific cluster.

		timestamp = Time.now.strftime("%Y-%m-%d %H:%M")
		newSourceDataDir = "#{$pgDataDirCustomer}.#{timestamp}.before_upgrade_to_postgres_#{$myPostgresVersion}"
		$logger.info("Moving previous database directory aside to : #{newSourceDataDir}")
		FileUtils.mv("#{$pgDataDirCustomer}", "#{newSourceDataDir}")

		# If the old data directory contains a .pid file due to Postgres not shutting down properly, get rid of the file so that we can attempt upgrade.
		# There should be no chance that a postmaster is actually using the old data directory at this point.
		if File.exists?(newSourceDataDir +  "/postmaster.pid")
			$logger.info("There is a .pid file in the source data dir.  Removing it to attempt upgrade.")
			FileUtils.rm_f(newSourceDataDir + "/postmaster.pid")
		end

		FileUtils.mkdir_p($pgDataDirCustomer)
		FileUtils.chmod(0700, $pgDataDirCustomer)
		FileUtils.chown("_postgres", "_postgres", $pgDataDirCustomer)

		$logger.info("Initializing the target customer-specific database cluster")
		command = "sudo -u _postgres #{$initdb} --encoding UTF8 --locale=C -D \"#{$pgDataDirCustomer}\""
		runCommandOrExit(command)

		$logger.info("Running pg_upgrade...")
		firstServer = TCPServer.new('127.0.0.1', 0)
		firstPort = firstServer.addr[1]
		secondServer = TCPServer.new('127.0.0.1', 0)
		secondPort = secondServer.addr[1]
		firstServer.close
		secondServer.close
		origWorkingDirectory = Dir.getwd
		Dir.chdir($migrationDir)
		command = "sudo -u _postgres #{$pg_upgrade} -b #{$postgresBinariesDir9_1} -B #{$newPostgresBinariesDir} -d \"#{newSourceDataDir}\" -D \"#{$pgDataDirCustomer}\" -p #{firstPort} -P #{secondPort}"
		ret = runCommand(command)
		if (ret != 0)
			Dir.chdir(origWorkingDirectory)
			exitWithError("Wiki and Profile Manager will not be available.")
		end
		Dir.chdir(origWorkingDirectory)

		FileUtils.mkdir_p($pgDataDir)
		FileUtils.chmod(0700, $pgDataDir)
		FileUtils.chown("_postgres", "_postgres", $pgDataDir)

		if File.exists?("#{newSourceDataDir}/global/pg_control.old")
			FileUtils.mv("#{newSourceDataDir}/global/pg_control.old", "#{newSourceDataDir}/global/pg_control")
		end

		forkDatabases($pgDataDir)
	end

	command = "#{$serveradmin} settings postgres_server:dataDir=\"#{$pgDataDir}\""
	runCommandOrExit(command)
	command = "#{$serveradmin} settings postgres:dataDir=\"#{$pgDataDirCustomer}\""
	runCommandOrExit(command)

	$logger.info("Stopping the customer instance of PostgreSQL")
	dictionaryFromServerAdmin("stop postgres")

	$logger.info("Restoring default listen_addresses setting for customer instance of PostgreSQL")
	command = "#{$serveradmin} settings postgres:listen_addresses=\"127.0.0.1,::1\""
	runCommandOrExit(command)

	# Leave the new postgres instance running.  Services should not have to start it.
end

exitWithMessage("Finished.")

