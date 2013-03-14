#!/usr/bin/ruby

# Copyright (c) 2012 Apple Inc. All Rights Reserved.

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
		ServerDefaultWebVHostConfigPath = "#{$SERVER_WEB_CONFIG_DIR}sites/0000_any_80_.conf"
		ServerDefaultWebSecureVHostConfigPath = "#{$SERVER_WEB_CONFIG_DIR}sites/0000_any_443_.conf"
		DesktopDefaultWebConfigPath = '/etc/apache2/httpd.conf'
		ServerAppPath = '/Applications/Server.app/Contents/MacOS/Server'
		ServerWordPressPath = "#{$SERVER_LIBRARY_PATH}/Web/Data/Sites/Default/wordpress"
		DesktopWordPressPath = '/Library/WebServer/Documents/wordpress'
		ThinPath = '/usr/bin/thin'
		MySQLPath = '/usr/local/mysql'
		MampPath = '/Applications/MAMP'
		XCodePath = '/Applications/Xcode.app'
		activeConfigFile = DesktopDefaultWebConfigPath
		config_dict = {
			'active' => 0,
			'valid_syntax' => 0,
			'invalid_syntax' => 0,
			'invalid_usage' => 0,
			'failed_open' => 0,
			'unexpected_response' => 0,
			'zero_exit_status' => 0,
			'nonzero_exit_status' => 0,
			'has_php_enabled' => 0,
			'has_perl_enabled' => 0,
			'has_ssl_enabled' => 0,
			'has_third_party_web_sw' => 0,
			'has_xcode' => 0
		}
		settings = {
			'apache_launch' => 1,
			'desktop_config' => config_dict.clone,
			'server_config' => config_dict.clone,
			'unknown_config' => config_dict.clone,
			'has_server_app_installed' => 0,
			'uses_pristine_desktop_config_file' => 0,
			'uses_pristine_server_main_config_file' => 0,
			'uses_pristine_server_vhost_config_file' => 0,
			'has_server_webservice_enabled' => 0,
			'has_wiki_enabled' => 0,
			'has_wsgi_enabled' => 0,
			'has_devicemanagement_enabled' => 0,
			'has_calendarserver_enabled' => 0,
			'has_webdavsharing_enabled' => 0,
			'has_custom_sites' => 0,
			'has_many_custom_sites' => 0
		}
		for i in 0..ARGV.count - 2
			if ARGV[i] == "-f"
				activeConfigFile = ARGV[i + 1]
			elsif ARGV[i] == "-D" && ARGV[i + 1] == "WEBSERVICE_ON"
				settings['has_server_webservice_enabled'] = 1
			end
		end
		sum = `/usr/bin/cksum #{activeConfigFile} 2>&1`.chomp.split(/\s+/)[0]
		if activeConfigFile == ServerDefaultWebConfigPath
			config = 'server_config'
			if ['2471706962'].include?(sum)
				settings['uses_pristine_server_main_config_file'] = 1
			end
		elsif activeConfigFile == DesktopDefaultWebConfigPath
			config = 'desktop_config'
			if ['3298336432'].include?(sum)
				settings['uses_pristine_desktop_config_file'] = 1
			end
		else
			config = 'unknown_config'
		end
		settings[config]['active'] = 1
		open(activeConfigFile) do |file|
			file.each_line do |line|
				if line.match(/^\s*LoadModule\s+php5_module/)
					settings[config]['has_php_enabled'] = 1
				elsif line.match(/^\s*LoadModule\s+perl_module/)
					settings[config]['has_perl_enabled'] = 1
				elsif config == 'server_config' && line.match(/^\s*Include\s+\/private\/etc\/apache2\/extra\/httpd-ssl.conf/)
					settings[config]['has_ssl_enabled'] = 1
				elsif config == 'unknown_config' && line.match(/^\s*SSLEngine\s+On/)
					settings[config]['has_ssl_enabled'] = 1
				end
			end
		end
		if config == 'server_config'
			sum = `/usr/bin/cksum #{ServerDefaultWebVHostConfigPath} 2>&1`.chomp.split(/\s+/)[0]
			if ['872795171', '1914927767'].include?(sum)
				settings['uses_pristine_server_vhost_config_file'] = 1
			else
				open(ServerDefaultWebVHostConfigPath) do |file|
					file.each_line do |line|
						if line.match(/^\s*Include\s+.*httpd_wsgi.conf/)
							settings['has_wsgi_enabled'] = 1
						elsif line.match(/^\s*Include\s+.*httpd_corecollaboration_required.conf/)
							settings['has_wiki_enabled'] = 1
						elsif line.match(/^\s*Include\s+.*httpd_devicemanagement.conf/)
							settings['has_devicemanagement_enabled'] = 1
						elsif line.match(/^\s*Include\s+.*httpd_calendarserver.conf/)
							settings['has_calendarserver_enabled'] = 1
						elsif line.match(/^\s*Include\s+.*httpd_webdavsharing.conf/)
							settings['has_webdavsharing_enabled'] = 1
						end
					end
				end
			end
			open(ServerDefaultWebSecureVHostConfigPath) do |file|
				file.each_line do |line|
					if line.match(/^\s*Include\s+.*httpd_wsgi.conf/)
						settings['has_wsgi_enabled'] = 1
					elsif line.match(/^\s*Include\s+.*httpd_corecollaboration_required.conf/)
						settings['has_wiki_enabled'] = 1
					elsif line.match(/^\s*Include\s+.*httpd_devicemanagement.conf/)
						settings['has_devicemanagement_enabled'] = 1
					elsif line.match(/^\s*Include\s+.*httpd_calendarserver.conf/)
						settings['has_calendarserver_enabled'] = 1
					elsif line.match(/^\s*Include\s+.*httpd_webdavsharing.conf/)
						settings['has_webdavsharing_enabled'] = 1
					end
				end
			end
			siteCount = Dir.glob("#{$SERVER_WEB_CONFIG_DIR}sites/*.conf").count
			settings['has_custom_sites'] = 1 if siteCount > 3
			settings['has_many_custom_sites'] = 1 if siteCount > 12
		end
		msg = `/usr/sbin/httpd #{ARGV.join(' ')} -t 2>&1`
		if msg =~ /^Syntax OK/
			settings[config]['valid_syntax'] = 1
		elsif msg =~ /^Syntax error/
			settings[config]['invalid_syntax'] = 1
		elsif msg =~ /^Usage:/
			settings[config]['invalid_usage'] = 1
		elsif msg =~ /Could not open configuration/
			settings[config]['failed_open'] = 1
		else
			settings[config]['unexpected_response'] = 1
		end
		settings[config]['zero_exit_status'] = $?.exitstatus == 0 ? 1 : 0
		settings[config]['nonzero_exit_status'] = $?.exitstatus == 0 ? 0 : 1
		settings['has_server_app_installed'] = FileTest.exists?(ServerAppPath) ? 1 : 0
		settings[config]['has_third_party_web_sw'] = (FileTest.exists?(ServerWordPressPath) || FileTest.exists?(DesktopWordPressPath) \
													  || FileTest.exists?(MySQLPath) \
													  || FileTest.exists?(MampPath) \
													  || FileTest.exists?(ThinPath)) ? 1 : 0
		settings[config]['has_xcode'] = FileTest.exists?(XCodePath) ? 1 : 0

		`syslog -s -l Notice -k com.apple.message.domain com.apple.server.apache.detailed.launch.stats \
			com.apple.message.apache_launch #{settings['apache_launch']} \
	\
			com.apple.message.uses_server_config #{settings['server_config']['active']} \
			com.apple.message.server_config_has_valid_syntax #{settings['server_config']['valid_syntax']} \
			com.apple.message.server_config_has_invalid_syntax #{settings['server_config']['invalid_syntax']} \
			com.apple.message.server_config_has_invalid_usage #{settings['server_config']['invalid_usage']} \
			com.apple.message.server_config_has_failed_open #{settings['server_config']['failed_open']} \
			com.apple.message.server_config_has_unexpected_response #{settings['server_config']['unexpected_response']} \
			com.apple.message.server_config_has_zero_exit_status #{settings['server_config']['zero_exit_status']} \
			com.apple.message.server_config_has_nonzero_exit_status #{settings['server_config']['nonzero_exit_status']} \
	\
			com.apple.message.uses_desktop_config #{settings['desktop_config']['active']} \
			com.apple.message.desktop_config_has_valid_syntax #{settings['desktop_config']['valid_syntax']} \
			com.apple.message.desktop_config_has_invalid_syntax #{settings['desktop_config']['invalid_syntax']} \
			com.apple.message.desktop_config_has_invalid_usage #{settings['desktop_config']['invalid_usage']} \
			com.apple.message.desktop_config_has_failed_open #{settings['desktop_config']['failed_open']} \
			com.apple.message.desktop_config_has_unexpected_response #{settings['desktop_config']['unexpected_response']} \
			com.apple.message.desktop_config_has_zero_exit_status #{settings['desktop_config']['zero_exit_status']} \
			com.apple.message.desktop_config_has_nonzero_exit_status #{settings['desktop_config']['nonzero_exit_status']} \
	\
			com.apple.message.uses_unknown_config #{settings['unknown_config']['active']} \
			com.apple.message.unknown_config_has_valid_syntax #{settings['unknown_config']['valid_syntax']} \
			com.apple.message.unknown_config_has_invalid_syntax #{settings['unknown_config']['invalid_syntax']} \
	\
			com.apple.message.uses_pristine_desktop_config_file #{settings['uses_pristine_desktop_config_file']} \
			com.apple.message.uses_pristine_server_main_config_file #{settings['uses_pristine_server_main_config_file']} \
			com.apple.message.uses_pristine_server_vhost_config_file #{settings['uses_pristine_server_vhost_config_file']} \
			com.apple.message.has_server_app_installed #{settings['has_server_app_installed']} \
			com.apple.message.desktop_config_with_php_enabled #{settings['desktop_config']['has_php_enabled']} \
			com.apple.message.server_config_with_php_enabled #{settings['server_config']['has_php_enabled']} \
			com.apple.message.desktop_config_with_perl_enabled #{settings['desktop_config']['has_perl_enabled']} \
			com.apple.message.server_config_with_perl_enabled #{settings['server_config']['has_perl_enabled']} \
			com.apple.message.server_third_party_web_sw_installed #{settings['server_config']['has_third_party_web_sw']} \
			com.apple.message.desktop_third_party_web_sw_installed #{settings['desktop_config']['has_third_party_web_sw']} \
			com.apple.message.unknown_third_party_web_sw_installed #{settings['unknown_config']['has_third_party_web_sw']} \
			com.apple.message.server_xcode_installed #{settings['server_config']['has_xcode']} \
			com.apple.message.desktop_xcode_installed #{settings['desktop_config']['has_xcode']} \
			com.apple.message.unknown_xcode_installed #{settings['unknown_config']['has_xcode']} \
			com.apple.message.has_wiki_enabled #{settings['has_wiki_enabled']} \
			com.apple.message.has_custom_sites #{settings['has_custom_sites']} \
			com.apple.message.has_many_custom_sites #{settings['has_many_custom_sites']} \
			com.apple.message.has_wsgi_enabled #{settings['has_wsgi_enabled']} \
			com.apple.message.has_devicemanagement_enabled #{settings['has_devicemanagement_enabled']} \
			com.apple.message.has_calendarserver_enabled #{settings['has_calendarserver_enabled']} \
			com.apple.message.has_webdavsharing_enabled #{settings['has_webdavsharing_enabled']} \
			com.apple.message.has_ssl_enabled_for_desktop #{settings['desktop_config']['has_ssl_enabled']} \
			com.apple.message.has_ssl_enabled_for_unknown #{settings['unknown_config']['has_ssl_enabled']} \
			com.apple.message.has_server_webservice_enabled #{settings['has_server_webservice_enabled']} \
	\
			com.apple.message.websites_only #{(settings['has_server_webservice_enabled'] == 1 \
				&& settings['has_wiki_enabled'] == 0 \
				&& settings['has_devicemanagement_enabled'] == 0 \
				&& settings['has_calendarserver_enabled'] == 0 \
				&& settings['has_webdavsharing_enabled'] == 0) ? 1 : 0} \
			com.apple.message.websites_or_a_service #{(settings['has_server_webservice_enabled'] == 1 \
				|| settings['has_wiki_enabled'] == 1 \
				|| settings['has_devicemanagement_enabled'] == 1 \
				|| settings['has_calendarserver_enabled'] == 1 \
				|| settings['has_webdavsharing_enabled'] == 1) ? 1 : 0} \
			com.apple.message.service_only #{(settings['has_server_webservice_enabled'] == 0 \
				&& settings['has_wiki_enabled'] == 1 \
				&& settings['has_devicemanagement_enabled'] == 1 \
				&& settings['has_calendarserver_enabled'] == 1 \
				&& settings['has_webdavsharing_enabled'] == 1) ? 1 : 0} \
			com.apple.message.no_websites_nor_service #{(settings['has_server_webservice_enabled'] == 0 \
				&& settings['has_wiki_enabled'] == 0 \
				&& settings['has_devicemanagement_enabled'] == 0 \
				&& settings['has_calendarserver_enabled'] == 0 \
				&& settings['has_webdavsharing_enabled'] == 0) ? 1 : 0}`
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
