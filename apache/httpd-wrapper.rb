#!/usr/bin/ruby

# Copyright (c) 2012, 2014-2015, 2017, 2019, 2021 Apple Inc. All Rights Reserved.

# IMPORTANT NOTE: This file is licensed only for use on Apple-branded
# computers and is subject to the terms and conditions of the Apple Software
# License Agreement accompanying the package this file is a part of.
# You may not port this file to another platform without Apple's written consent.

# This is a wrapper script for httpd which sets certain
# environment variables and "-D" settings used by parameterized 
# Apache config files, based on the contents of the env.plist file.

# This script also gathers minimal usage data on the Apache web server. It logs 
# usage data locally, but that data is only sent to Apple if the "send usage data" 
# option is turned on.

require 'fileutils'
def main
    begin
        require 'cfpropertylist'
        args = ARGV
        if FileTest.exist?(WFS_CONFIG_FILE_PATH)
            if FileTest.exist?(ENV_PLIST_PATH)
                envPlist = CFPropertyList::List.new(:file => ENV_PLIST_PATH)
                envDict = CFPropertyList.native_types(envPlist.value)
                envDict.each do |key, value|
                    if key.start_with? '-D'
                        args.unshift(key)
                    else
                        ENV[key] = value
                    end
                end
            else
                File.rename(WFS_CONFIG_FILE_PATH, WFS_CONFIG_FILE_PATH + '.inactive')
            end
        end
        if !FileTest.exist?(LOG_DIR)
        	FileUtils.mkdir_p(LOG_DIR)
        end
        if !FileTest.exist?(LAST_USE_FILE) || Time.now - File.new(LAST_USE_FILE).atime > TOO_SOON_IN_SECONDS
            metrics = Metrics.new
            metrics.gather
            metrics.log
        end
        exec("#{HTTPD_PATH} #{ARGV.join(' ')}")
    rescue => e
        require 'logger'
        $logger = Logger.new('/var/log/apache2/httpd-wrapper.log')
        $logger.level = Logger::ERROR
        $logger.error("Exception raised running httpd-wrapper: #{e.message}")
        $logger.error("Proceeding with exec of httpd #{ARGV.join(' ')}")
        exec("#{HTTPD_PATH} #{ARGV.join(' ')}")
    end
end

class Modules
    attr_reader :activeModules
    def initialize
        @defaultActiveModules = %w[core_module so_module http_module mpm_prefork_module authn_file_module authn_core_module authz_host_module authz_groupfile_module authz_user_module authz_core_module access_compat_module auth_basic_module reqtimeout_module filter_module mime_module log_config_module env_module headers_module setenvif_module version_module slotmem_shm_module unixd_module status_module autoindex_module negotiation_module dir_module alias_module hfs_apple_module]
        @defaultModules = %w[core_module socache_redis_module so_module http_module mpm_event_module mpm_prefork_module mpm_worker_module authn_file_module authn_dbm_module authn_anon_module authn_dbd_module authn_socache_module authn_core_module authz_host_module authz_groupfile_module authz_user_module authz_dbm_module authz_owner_module authz_dbd_module authz_core_module authnz_ldap_module access_compat_module auth_basic_module auth_form_module auth_digest_module allowmethods_module file_cache_module cache_module cache_disk_module cache_socache_module socache_shmcb_module socache_dbm_module socache_memcache_module watchdog_module macro_module dbd_module dumpio_module echo_module buffer_module data_module ratelimit_module reqtimeout_module ext_filter_module request_module include_module filter_module reflector_module substitute_module sed_module charset_lite_module deflate_module xml2enc_module proxy_html_module mime_module ldap_module log_config_module log_debug_module log_forensic_module logio_module env_module mime_magic_module expires_module headers_module usertrack_module unique_id_module setenvif_module version_module remoteip_module proxy_module proxy_connect_module proxy_ftp_module proxy_http_module proxy_fcgi_module proxy_scgi_module proxy_uwsgi_module proxy_fdpass_module proxy_wstunnel_module proxy_ajp_module proxy_balancer_module proxy_express_module proxy_hcheck_module session_module session_cookie_module session_dbd_module slotmem_shm_module slotmem_plain_module ssl_module dialup_module http2_module lbmethod_byrequests_module lbmethod_bytraffic_module lbmethod_bybusyness_module lbmethod_heartbeat_module unixd_module heartbeat_module heartmonitor_module dav_module status_module autoindex_module asis_module info_module dav_fs_module dav_lock_module vhost_alias_module negotiation_module dir_module imagemap_module actions_module speling_module userdir_module alias_module rewrite_module php7_module perl_module hfs_apple_module authnz_od_apple_module]
        @activeModules = []
        rawString = `#{HTTPD_PATH}  #{ARGV.join(' ')} -M 2>/dev/null`.split("\n")
        rawString.each do | line |
            @activeModules << line.sub(/ \(.*$/, '').strip unless line =~ /Loaded Modules/
        end
    end
    def customerInstalledModules
        return @activeModules - @defaultModules
    end
    def customerDeActivatedStandardModules
        return @defaultActiveModules - @activeModules
    end
    def customerActivatedStandardModules
        return @activeModules - @defaultActiveModules - customerInstalledModules
    end
end
                                       
class Metrics
    def gather
        require 'fileutils'
        FileUtils.touch(LAST_USE_FILE)
        activeConfigFile = DEFAULT_WEB_CONFIG_PATH
        @settings = {
            'apache_launch' => 1
        }
        sum = `/usr/bin/cksum #{activeConfigFile} 2>&1`.chomp.split(/\s+/)[0]
        @settings['pristine_config_file'] = (['3040154737'].include?(sum)) ? 1 : 0
        @settings["non_standard_document_root"] = 0
        rawString = `#{HTTPD_PATH} #{ARGV.join(' ')} -S 2>/dev/null`.split("\n")
        rawString.each do | line |
            if line =~ /Main DocumentRoot:/
                if line !~ /\"\/Library\/WebServer\/Documents\"$/
                    @settings["non_standard_document_root"] = 1
                end
                break
            end
        end
        modules = Modules.new
        @settings['php_enabled'] = modules.activeModules.include?('php7_module') ? 1 : 0
        @settings['perl_enabled'] = modules.activeModules.include?('perl_module') ? 1 : 0
        @settings['non_standard_mpm_enabled'] = modules.activeModules.include?('mpm_prefork_module') ? 0 : 1
        @settings['http2_enabled'] = modules.activeModules.include?('http2_module') ? 1 : 0
        @settings['ssl_enabled'] = modules.activeModules.include?('ssl_module') ? 1 : 0
        @settings['webdavsharing_enabled'] = modules.activeModules.include?('authnz_od_apple_module') ? 1 : 0
        @settings['enabled_custom_module'] = (modules.customerInstalledModules.count > 0) ? 1 : 0
        @settings['disabled_normally_enabled_standard_module'] = (modules.customerDeActivatedStandardModules.count > 0) ? 1 : 0
        @settings['enabled_normally_disabled_standard_module'] = (modules.customerActivatedStandardModules.count > 0) ? 1 : 0
        msg = `#{HTTPD_PATH} #{ARGV.join(' ')} -t 2>&1`
        @settings['valid_syntax'] = (msg =~ /^Syntax OK/) ? 1 : 0
        @settings['server_app_installed'] = FileTest.exist?(SERVER_APP_PATH) ? 1 : 0
    end
    def log
        `syslog -s -l Notice -k com.apple.message.domain com.apple.apache.launch.stats \
            com.apple.message.apache_launch #{@settings['apache_launch']} \
            com.apple.message.valid_syntax #{@settings['valid_syntax']} \
            com.apple.message.pristine_config_file #{@settings['pristine_config_file']} \
            com.apple.message.server_app_installed #{@settings['server_app_installed']} \
            com.apple.message.php_enabled #{@settings['php_enabled']} \
            com.apple.message.perl_enabled #{@settings['perl_enabled']} \
            com.apple.message.webdavsharing_enabled #{@settings['webdavsharing_enabled']} \
            com.apple.message.enabled_custom_module #{@settings['enabled_custom_module']} \
            com.apple.message.disabled_normally_enabled_standard_module #{@settings['disabled_normally_enabled_standard_module']} \
            com.apple.message.enabled_normally_disabled_standard_module #{@settings['enabled_normally_disabled_standard_module']} \
            com.apple.message.non_standard_mpm_enabled #{@settings['non_standard_mpm_enabled']} \
            com.apple.message.http2_enabled #{@settings['http2_enabled']} \
            com.apple.message.non_standard_document_root #{@settings['non_standard_document_root']} \
       com.apple.message.ssl_enabled #{@settings['ssl_enabled']}`
    end
end
                                       
ENV_PLIST_PATH = '/etc/apache2/env.plist'
WFS_CONFIG_FILE_PATH = '/etc/apache2/other/httpd_webdavsharing.conf'
HTTPD_PATH = '/usr/sbin/httpd'
DEFAULT_WEB_CONFIG_PATH = '/etc/apache2/httpd.conf'
SERVER_APP_PATH = 'Applications/Server.app'
LOG_DIR = '/var/log/apache2'
LAST_USE_FILE = '/var/db/.httpd-wrapper'
TOO_SOON_IN_SECONDS = 3600.0        # Throttle

main
