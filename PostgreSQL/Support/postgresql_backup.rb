#!/usr/bin/ruby 
#
# postgresql_backup.rb - PostgreSQL Service backup and restore plugin for ServerBackup.
# Consolidates the _backup, _restore, and _verify functions in a single tool
#
# Author:: Apple Inc.
# Documentation:: Apple Inc.
# Copyright © 2011, Apple Inc.
# License:: All rights reserved.
#

require 'digest'
require 'ftools'
require 'logger'
require 'optparse'
require 'ostruct'
require 'shellwords'

$: << File.dirname(File.expand_path(__FILE__))
require 'backuptool'
require 'sysexits'

include SysExits


# == Apple-internal documentation
#
# == PostgreSQLTool
#
# === Description
#
# PostgreSQLTool is a subclass of BackupTool, processing PostgreSQL-specific
# functionality for ServerBackup(8).
#
# === Further documentation
#
# For information on how this class is typically invoked, see ServerBackup(8).
#
class PostgreSQLTool < BackupTool
	#
	# Constants
	#
	BACKUP_DIR = "/Library/Server/PostgreSQL/Backup"
	BACKUP_FILE = "dumpall.psql.gz"
	DB_DIR = "/private/var/pgsql"
	SECRET_DIR = "/.ServerBackups/postgresql"
	LOG_DIR = "/Library/Logs"
	LOG_FILE = "PostgreSQL.log"
	SOCKET_DIR = "/var/pgsql_socket"

	#
	# Class Methods
	#
	def initialize
		super("postgres", "1.2")
		self
	end

	#
	# Instance Methods
	#

	# Get the current database location
	def dataDir
		dataDir = self.setting("postgres:dataDir")
		if (dataDir.nil? || dataDir.empty? || dataDir["/var/pgsql"])
			$log.warn("Error determining data directory; using default.")
			return DB_DIR
		end
		$log.debug("Service data directory is #{dataDir}")
		return dataDir
	end

	# Get the current database backup location
	def backupDir
		dataDir = self.dataDir
		if (dataDir.nil? || dataDir.empty? || dataDir["/var/pgsql"])
			return BACKUP_DIR
		end
		return dataDir.sub(/Data\z/, "Backup")
	end

	# Get the current log file
	def logFile
		logDir = self.setting("postgres:log_directory", LOG_DIR)
		logFile = self.setting("postgres:log_filename", LOG_FILE)
		$log.debug("Service log file is #{logDir}/#{logFile}")
		return "#{logDir}/#{logFile}"
	end

	# Get the current socket directory.
	def socketDir
		return self.setting("postgres:unix_socket_directory", SOCKET_DIR)
	end

	# Primary operations

	# Validate arguments and backup this service
	def backup
		unless (@options && @options[:path] && @options[:dataset])
			raise OptionParser::InvalidArgument, "Missing arguments for 'backup'."
		end
		$log.debug("@options = #{@options.inspect}")
		archive_dir = @options[:path]
		unless (archive_dir[0] == ?/)
			raise OptionParser::InvalidArgument, "Paths must be absolute."
		end
		what = @options[:dataset]
		unless self.class::DATASETS.include?(what)
			raise OptionParser::InvalidArgument, "Unknown data set '#{@options[:dataset]}' specified."
		end
		# The passed :archive_dir and :what are ignored because the dump is put
		# on the live data volume
		archive_dir = self.backupDir
		dump_file = "#{archive_dir}/#{BACKUP_FILE}"
		# Create the backup directory as necessary.
		unless File.directory?(archive_dir)
			if File.exists?(archive_dir)
				$log.info "Moving aside #{archive_dir}...\n"
				FileUtils.mv(archive_dir, archive_dir + ".applesaved")
			end
			$log.info "Creating backup directory: #{archive_dir}...\n"
			FileUtils.mkdir_p(archive_dir, :mode => 0700)
			# _postgres:_postgres has uid:gid of 216:216
			File.chown(216, 216, archive_dir)
		end
		# Backup only once a day
		mod_time = File.exists?(dump_file) ? File.mtime(dump_file) : Time.at(0)
		if (Time.now - mod_time) >= (24 * 60 * 60)
			$log.info "Creating dump file \'#{dump_file}\'..."
			system("/usr/bin/sudo -u _postgres /usr/bin/pg_dumpall | /usr/bin/gzip > #{dump_file.shellescape}")
			if ($?.exitstatus == 0)
				File.chmod(0640, dump_file)
				File.chown(216, 216, dump_file)
				$log.info "...Backup succeeded."
			else
				$log.error "...Backup failed! Status=#{$?.exitstatus}"
			end
		else
			$log.info "Dump file is less than 24 hours old; skipping."
		end
	end

	# Validate arguments and verify that the backup archive matches the file system.
	def verify
		unless (@options && @options[:path] && @options[:target])
			raise OptionParser::InvalidArgument, "Missing arguments for 'verify'."
		end
		$log.debug("@options = #{@options.inspect}")
		source_dir = @options[:path]
		unless (source_dir[0] == ?/)
			raise OptionParser::InvalidArgument, "Paths must be absolute."
		end
		# Point to the root volume if :path points to the secret restore path.
		if (source_dir == SECRET_DIR)
			source_dir = ""
		end
		# Bail if the restore file is not present.
		archive_dir = self.backupDir
		dump_file = "#{source_dir}#{archive_dir}/#{BACKUP_FILE}"
		unless File.file?("#{dump_file}")
			raise RuntimeError, "Backup file not present in source volume."
		end
		target = @options[:target]
		unless (target == "/")
			raise RuntimeError, "Backups can only be verified against a running service."
		end
		digest_disk = Digest::SHA256.file("#{dump_file}")
		digest_live = Digest::SHA256.new
		open("|/usr/bin/sudo -u _postgres /usr/bin/pg_dumpall | /usr/bin/gzip") do |f|
			buf = ""
			while f.read(16384, buf)
				digest_live << buf
			end
		end
		return (digest_disk == digest_live)
	end

	# Validate arguments and restore this service
	def restore
		unless (@options && @options[:path] && @options[:dataset] && @options[:target])
			raise OptionParser::InvalidArgument, "Missing arguments for 'restore'."
		end
		$log.debug("@options = #{@options.inspect}")
		source_dir = @options[:path]
		unless (source_dir[0] == ?/)
			raise OptionParser::InvalidArgument, "Paths must be absolute."
		end
		what = @options[:dataset]
		unless self.class::DATASETS.include?(what)
			raise OptionParser::InvalidArgument, "Unknown data set '#{@options[:dataset]}' specified."
		end
		if (what.to_sym == :configuration)
			$log.info "Configuration is part of the data set; nothing to restore."
			return
		end
		target = @options[:target]
		unless (target == "/")
			raise RuntimeError, "Databases can only be restored to a running service."
		end

		# Point to the root volume if :path points to the secret restore path.
		if (source_dir == SECRET_DIR)
			source_dir = ""
		end

		# Create the log file if it doesn't exist.
		log_file = self.logFile
		if !File.exists?(log_file)
			$log.warn "Recreating #{log_file}."
			FileUtils.touch(log_file)
		end
		# Always ensure the permissions & ownership are correct.
		FileUtils.chmod(0660, log_file)
		# _postgres has uid of 216; using instead of string in case user db hasn't yet been restored
		FileUtils.chown(216, "admin", log_file)

		# Create the socket directory if it doesn't exist.
		socket_dir = self.socketDir
		if !File.exists?(socket_dir)
			$log.warn "Recreating #{socket_dir}."
			FileUtils.mkdir_p(socket_dir, :mode => 0750)
			FileUtils.chown(216, 216, socket_dir)
		end

		# Bail if the restore file is not present.
		archive_dir = self.backupDir
		dump_file = "#{source_dir}#{archive_dir}/#{BACKUP_FILE}"
		$log.info "Restoring \'#{dump_file}\' to \'#{target}\'..."
		unless File.file?(dump_file)
			raise RuntimeError, "Backup file not present in source volume! Nothing to restore!"
		end

		# Recall if the service was previously enabled
		db_dir = self.dataDir
		state = false
		self.launch("/usr/sbin/serveradmin status postgres") do |output|
			state = ((/RUNNING/ =~ output) != nil)
		end
		self.launch("/usr/sbin/serveradmin stop postgres") if state
		if (File.directory?(db_dir))
			$log.info "...moving aside previous database..."
			FileUtils.mv(db_dir, "#{db_dir}.pre-restore-#{Time.now.strftime('%Y-%m-%d_%H:%M:%S_%Z')}")
		end
		$log.info "...creating an empty database at #{db_dir}..."
		FileUtils.mkdir_p(db_dir, :mode => 0700)
		# _postgres:_postgres has uid:gid of 216:216
		File.chown(216, 216, db_dir)
		self.launch("/usr/bin/sudo -u _postgres /usr/bin/initdb --encoding UTF8 -D #{db_dir.shellescape}")
		self.launch("/usr/sbin/serveradmin start postgres")
		$log.info "...replaying database contents (this may take a while)..."
		system("/usr/bin/gzcat #{dump_file.shellescape} | /usr/bin/sudo -u _postgres /usr/bin/psql postgres")
		self.launch("/usr/sbin/serveradmin stop postgres") unless state
		$log.info "...Restore succeeded."
	end
end

tool = PostgreSQLTool.new
$log.level = Logger::INFO
$logerr.level = Logger::INFO
begin
	tool.parse!(ARGV)
	status = tool.run
rescue OptionParser::InvalidArgument => exc
	$log.error "#{exc.to_s.capitalize}\n\n"
	tool.usage
	exit EX_USAGE
rescue RuntimeError => exc
	$log.error "#{exc.to_s.capitalize}\n"
	exit EX_UNAVAILABLE
rescue Exception => exc
	$log.error "#{exc.to_s.capitalize}\n"
	exit EX_UNAVAILABLE
rescue
	$log.error "unknown exception thrown\n"
	exit EX_UNAVAILABLE
end
exit (status ? EX_OK : EX_UNAVAILABLE)
