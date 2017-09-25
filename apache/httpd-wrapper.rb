#!/usr/bin/ruby

# Copyright (c) 2012, 2014-2015, 2017 Apple Inc. All Rights Reserved.

# IMPORTANT NOTE: This file is licensed only for use on Apple-branded
# computers and is subject to the terms and conditions of the Apple Software
# License Agreement accompanying the package this file is a part of.
# You may not port this file to another platform without Apple's written consent.
# This is a wrapper script for httpd which sets certain
# environment variables and "-D" settings used by parameterized 
# Apache config filesr, based on the contents of the env.plist file.

begin
    require 'cfpropertylist'
    Executable = '/usr/sbin/httpd'
    EnvPlistPath = '/etc/apache2/env.plist'
    args = ARGV
    if FileTest.exists?(EnvPlistPath)
        envPlist = CFPropertyList::List.new(:file => EnvPlistPath)
        envDict = CFPropertyList.native_types(envPlist.value)
        envDict.each do |key, value|
            if key.start_with? '-D'
                args.unshift(key)
            else
                ENV[key] = value 
            end
        end
    end
    exec("#{Executable} #{ARGV.join(' ')}")
rescue => e
    require 'logger'
    $logger = Logger.new('/var/log/apache2/httpd-wrapper.log')
    $logger.level = Logger::ERROR
    $logger.error("Exception raised running #{$0}: #{e.message}")
    $logger.error("Proceeding with exec of #{Executable}")
    exec("#{Executable} #{ARGV.join(' ')}")
end
