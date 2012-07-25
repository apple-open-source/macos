#!/usr/bin/ruby

# Copyright (c) 2011-2012 Apple Inc. All Rights Reserved.

# IMPORTANT NOTE: This file is licensed only for use on Apple-branded
# computers and is subject to the terms and conditions of the Apple Software
# License Agreement accompanying the package this file is a part of.
# You may not port this file to another platform without Apple's written consent.

require 'osx/foundation'
include OSX
$SERVER_INSTALL_PATH_PREFIX = "/Applications/Server.app/Contents/ServerRoot"

$SERVER_LIBRARY_PATH = "/Library/Server"
$SERVER_WEB_CONFIG_DIR = "#{$SERVER_LIBRARY_PATH}/Web/Config/apache2/"
$ServerAppWebConfigPath = "#{$SERVER_WEB_CONFIG_DIR}httpd_server_app.conf"

class WebPromotion
	def initialize
		@launchKey = "org.apache.httpd"
		@plistFile = "/System/Library/LaunchDaemons/#{@launchKey}.plist"
		@plistDict = NSDictionary.dictionaryWithContentsOfFile(@plistFile).mutableCopy
		@args = @plistDict["ProgramArguments"]
		@envVarDict = @plistDict["EnvironmentVariables"]
		if @envVarDict.nil?
			@envVarDict = {}
		else
			@envVarDict = @envVarDict.mutableCopy
		end
	end
	def apacheIsRunning
		`/bin/launchctl list #{@launchKey} >/dev/null 2>&1`
		return $?.exitstatus == 0
	end
	def promote
		if statusString != "SERVER_APP" && FileTest.directory?($SERVER_INSTALL_PATH_PREFIX)
			@args <<"-f"<<$ServerAppWebConfigPath
			if @envVarDict.nil?
				@envVarDict = {}
			end
			@envVarDict["SERVER_INSTALL_PATH_PREFIX"] = $SERVER_INSTALL_PATH_PREFIX
			@plistDict["EnvironmentVariables"] = @envVarDict
			@plistDict.writeToFile_atomically_(@plistFile, true)
			`/usr/sbin/apachectl restart` if apacheIsRunning
			`ln -s /var/log/apache2 /Library/Logs/WebServer`
		end
	end
	def demote
		if statusString != "DESKTOP"
			index = @args.index($ServerAppWebConfigPath)
			if !index.nil?
				@args.delete_at(index)
				@args.delete_at(index - 1)
			end
			if @envVarDict && !@envVarDict["SERVER_INSTALL_PATH_PREFIX"].nil?
				@envVarDict.delete("SERVER_INSTALL_PATH_PREFIX")
			end			
			if @envVarDict.empty?
				@plistDict.delete("EnvironmentVariables")
			else
				@plistDict["EnvironmentVariables"] = @envVarDict
			end
			@plistDict.writeToFile_atomically_(@plistFile, true)
			`/usr/sbin/apachectl restart` if apacheIsRunning 
			`rm /Library/Logs/WebServer`
		end
	end
	def status
		$stdout.print("#{statusString}\n")
	end
	def statusString
		if @args.join(' ').include?("-f #{$ServerAppWebConfigPath}") 
			return "SERVER_APP"
		else
			return "DESKTOP"
		end
	end
end

usage = <<EOU
Manage promotion of desktop to server or demotion from server to desktop, for web service

usage: #{File.basename($0)} promote|demote|status 

EOU

if Process.euid != 0
	$stderr.puts(usage)
	raise "Must run as root"
end
if ARGV.count == 0
	$stderr.puts(usage)
	raise ArgumentError, "Invalid arg count"
end
action = ARGV[0]

c = WebPromotion.new
if c.respond_to?(action)
	c.send(action)
else
	$stderr.puts(usage)
	raise ArgumentError, "#{action}: unknown action"
end

