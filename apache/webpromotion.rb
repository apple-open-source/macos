#!/usr/bin/ruby

# Copyright (c) 2011-2013 Apple Inc. All Rights Reserved.

# IMPORTANT NOTE: This file is licensed only for use on Apple-branded
# computers and is subject to the terms and conditions of the Apple Software
# License Agreement accompanying the package this file is a part of.
# You may not port this file to another platform without Apple's written consent.
require 'rubygems'
require 'cfpropertylist'
$SERVER_INSTALL_PATH_PREFIX = "/Applications/Server.app/Contents/ServerRoot"
$SERVER_LIBRARY_PATH = "/Library/Server"
$SERVER_WEB_CONFIG_DIR = "#{$SERVER_LIBRARY_PATH}/Web/Config/apache2/"
$ServerAppWebConfigPath = "#{$SERVER_WEB_CONFIG_DIR}httpd_server_app.conf"

# If Server has its own Apache, the desktop launchd.plist does not require modification on promote/demote.
$ServerAppHTTPDPath = "#{$SERVER_INSTALL_PATH_PREFIX}/usr/sbin/httpd"
$PromotionAffectsLanchdPlist = !FileTest.exists?($ServerAppHTTPDPath)

class WebPromotion
	def initialize
		@launchKey = "org.apache.httpd"
		@plistFile = "/System/Library/LaunchDaemons/#{@launchKey}.plist"
		plist = CFPropertyList::List.new(:file => @plistFile)
		@plistDict = CFPropertyList.native_types(plist.value)
		@args = @plistDict["ProgramArguments"]
		@envVarDict = @plistDict["EnvironmentVariables"]
		if @envVarDict.nil?
			@envVarDict = {}
		else
			@envVarDict = @envVarDict
		end
		@apacheWasRunning = apacheIsRunning
	end
	def apacheIsRunning
		`/bin/launchctl list #{@launchKey} >/dev/null 2>&1`
		return $?.exitstatus == 0
	end
	def promote
		if $PromotionAffectsLanchdPlist
			if statusString != "SERVER_APP" && FileTest.directory?($SERVER_INSTALL_PATH_PREFIX)
				`/bin/launchctl unload -w #{@plistFile}` if @apacheWasRunning
				@args <<"-f"<<$ServerAppWebConfigPath
				if @envVarDict.nil?
					@envVarDict = {}
				end
				@envVarDict["SERVER_INSTALL_PATH_PREFIX"] = $SERVER_INSTALL_PATH_PREFIX
				@plistDict["EnvironmentVariables"] = @envVarDict
				plist = CFPropertyList::List.new
				plist.value = CFPropertyList.guess(@plistDict)
				plist.save(@plistFile, CFPropertyList::List::FORMAT_XML)
				`/usr/bin/plutil -convert xml1 #{@plistFile}`	# Make file human-readable
				`/bin/launchctl load -w #{@plistFile}` if @apacheWasRunning
			end
		end
		`rm -f /Library/Logs/WebServer`
		`ln -s /var/log/apache2 /Library/Logs/WebServer`
	end
	def demote
		if $PromotionAffectsLanchdPlist
			if statusString != "DESKTOP"
				`/bin/launchctl unload -w #{@plistFile}` if @apacheWasRunning
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
				plist = CFPropertyList::List.new
				plist.value = CFPropertyList.guess(@plistDict)
				plist.save(@plistFile, CFPropertyList::List::FORMAT_XML)
				`/usr/bin/plutil -convert xml1 #{@plistFile}`	# Make file human-readable
				`/bin/launchctl load -w #{@plistFile}` if @apacheWasRunning
			end
		end
		`rm -f /Library/Logs/WebServer`
	end
	def status
		$stdout.print("#{statusString}\n")
	end
	def statusString
		if $PromotionAffectsLanchdPlist
			if @args.join(' ').include?("-f #{$ServerAppWebConfigPath}")
				return "SERVER_APP"
			else
				return "DESKTOP"
			end
		else
			if FileTest.exists?("/Library/Logs/WebServer")
				return "SERVER_APP"
			else
				return "DESKTOP"
			end
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

