#!/usr/bin/ruby

# Copyright (c) 2012, 2014 Apple Inc. All Rights Reserved.

# IMPORTANT NOTE: This file is licensed only for use on Apple-branded
# computers and is subject to the terms and conditions of the Apple Software
# License Agreement accompanying the package this file is a part of.
# You may not port this file to another platform without Apple's written consent.

# This is a wrapper script for httpd, intended to gather minimal usage data on
# the Apache web server. This script logs usage data locally, but that data is only
# sent to Apple if the "send usage data" option is turned on.

begin
	LastUseFile = "/var/db/.httpd-wrapper"
	TooSoonInSeconds = 3600.0		# Throttle
	if !FileTest.exists?(LastUseFile) || Time.now - File.new(LastUseFile).atime > TooSoonInSeconds
		require 'fileutils'
		FileUtils.touch(LastUseFile)
		$SERVER_LIBRARY_PATH = '/Library/Server'
		$SERVER_WEB_CONFIG_DIR = "#{$SERVER_LIBRARY_PATH}/Web/Config/apache2/"
		ServerDefaultWebConfigPath = "#{$SERVER_WEB_CONFIG_DIR}httpd_server_app.conf"
		DesktopDefaultWebConfigPath = '/etc/apache2/httpd.conf'
		ServerAppPath = '/Applications/Server.app/Contents/MacOS/Server'
		activeConfigFile = DesktopDefaultWebConfigPath
		settings = {
			'apache_launch' => 1,
			'config_has_valid_syntax' => 0,
			'uses_server_config' => 0,
			'has_server_app_installed' => 0,
			'uses_pristine_desktop_config_file' => 0,
			'has_server_webservice_enabled' => 0,
			'has_php_enabled' => 0,
			'has_perl_enabled' => 0,
			'has_ssl_enabled_for_desktop' => 0
		}
		for i in 0..ARGV.count - 2
			if ARGV[i] == "-f"
				activeConfigFile = ARGV[i + 1]
			elsif ARGV[i] == "-D" && ARGV[i + 1] == "WEBSERVICE_ON"
				settings['has_server_webservice_enabled'] = 1
			end
		end
		if activeConfigFile == ServerDefaultWebConfigPath
			settings['uses_server_config'] = 1
		elsif activeConfigFile == DesktopDefaultWebConfigPath
			sum = `/usr/bin/cksum #{activeConfigFile} 2>&1`.chomp.split(/\s+/)[0]
			if ['725083195'].include?(sum)
				settings['uses_pristine_desktop_config_file'] = 1
			end
		end
		open(activeConfigFile) do |file|
			file.each_line do |line|
				if line.match(/^LoadModule php5_module/)
					settings['has_php_enabled'] = 1
				elsif line.match(/^LoadModule perl_module/)
					settings['has_perl_enabled'] = 1
				elsif settings['uses_server_config'] == 0 && line.match(/^Include \/private\/etc\/apache2\/extra\/httpd-ssl.conf/)
					settings['has_ssl_enabled_for_desktop'] = 1
				end
			end
		end
		`/usr/sbin/httpd #{ARGV.join(' ')} -t 2>&1`
		settings['config_has_valid_syntax'] = $?.exitstatus == 0 ? 1 : 0
		settings['has_server_app_installed'] = FileTest.exists?(ServerAppPath) ? 1 : 0
		`syslog -s -l Notice -k com.apple.message.domain com.apple.server.apache.launch.stats \
			com.apple.message.apache_launch #{settings['apache_launch']} \
			com.apple.message.config_has_valid_syntax #{settings['config_has_valid_syntax']} \
			com.apple.message.uses_server_config #{settings['uses_server_config']} \
			com.apple.message.uses_pristine_desktop_config_file #{settings['uses_pristine_desktop_config_file']} \
			com.apple.message.has_server_app_installed #{settings['has_server_app_installed']} \
			com.apple.message.has_php_enabled #{settings['has_php_enabled']} \
			com.apple.message.has_perl_enabled #{settings['has_perl_enabled']} \
			com.apple.message.has_ssl_enabled_for_desktop #{settings['has_ssl_enabled_for_desktop']} \
			com.apple.message.has_server_webservice_enabled #{settings['has_server_webservice_enabled']}`
	end
	exec("/usr/sbin/httpd #{ARGV.join(' ')}")
rescue => e
	require 'logger'
	$logger = Logger.new('/var/log/apache2/httpd-wrapper.log')
	$logger.level = Logger::ERROR
	$logger.error("Exception raised running httpd-wrapper: #{e.message}")
	$logger.error("Proceeding with exec of httpd")
	exec("/usr/sbin/httpd #{ARGV.join(' ')}")
end
