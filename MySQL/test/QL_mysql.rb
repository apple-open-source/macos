#!/usr/bin/env ruby -w

# Name:        QL_mysql.rb
# Author:      Steve Peralta
# Date:        9/15/07
# Description: Quicklook test tool for MySQL service
#
# Usage:       QL_mysql --help
# Output:      /tmp/QL_mysql/QL_mysql.log

# INSTALLATION OF QuickLookTool.rb
#   Copy the QuickLookTool.rb file to the folder (based on system version):
#   10.5:  /usr/lib/ruby/site_ruby/1.8/universal-darwin9.0/
#   10.6:  /usr/lib/ruby/site_ruby/1.8/universal-darwin10.0/

require 'QuickLookTool'

#------------------------------------------------------------
# MySQLTool
#
# QuickLookTool class for MySQL
#------------------------------------------------------------

class MySQLTool < QuickLookTool

	TOOL_NAME = "QL_mysql"
	TOOL_VERSION = "1.8"
	SERVICE_NAME = "MySQL"
	SERVICE_ID = "mysql"
	SERVICE_DESC = "MySQL Quicklook Test Suite"

	MYSQL_DAEMON = "/usr/libexec/mysqld"
	MYSQL_TOOL = "/usr/bin/mysql"
	TEST_PWD = "admin"

	MT_PREFIX = "#{MYSQL_TOOL} -S /var/mysql/mysql.sock --password=#{TEST_PWD}"
	RESET_PWD_CMD = "#{MT_PREFIX} -e \"update mysql.user set password = PASSWORD('') where user='root';\""
	QUERY_USER_CMD = "#{MT_PREFIX} -e \"select Host,User from mysql.user;\""
	QUERY_USER__PWD_CMD = "#{MT_PREFIX} -u root -p -e \"select Host,User from mysql.user;\""
	MA_VARS_CMD = "mysqladmin -S /var/mysql/mysql.sock variables"

	#----------------------------------------
	def initialize
		super(TOOL_NAME, TOOL_VERSION, SERVICE_NAME, SERVICE_ID, SERVICE_DESC)

		# set additional execution prerequisites
		self.setPrereqFiles([SACommand::SA_TOOL, MYSQL_DAEMON])
		self.setTestCases([ 
			StopService, 
			StartService,
			SetDatabaseLocation, 
			VerifyDatabaseLocation, 
			RestoreDatabaseLocation, 
			DisableAllowNetworking, 
			VerifyDisableAllowNetworking,
			EnableAllowNetworking, 
			VerifyEnableAllowNetworking, 
			RestoreAllowNetworking,
			SetRootPassword, 
			VerifyRootPassword, 
			RestoreRootPassword,
		])

		# initialize data unique to this service
		@dataDirPath = "#{self.logDir}/data"

		# create utility instance for accessing serveradmin functions
		@saCommand = SACommand.new(self, "", nil, nil)

		@savedAllowNet = ""
		@savedDataDir = ""
	end

	#----------------------------------------
	def dataDirPath
		@dataDirPath
	end

	#----------------------------------------
	def resetRootPassword
		# Note: this method assumes the password was last set to MySQLTool::TEST_PWD
		return if self.debugMode
		`#{MySQLTool::RESET_PWD_CMD}`
	end
	
	#----------------------------------------
	def restoreDefaultSetting(aKey, aVal)
		self.logAndShow("- restoring default setting: #{aKey}")
		@saCommand.writeSetting("#{aKey}", "#{aVal}")
	end
	
	#----------------------------------------
	def doTestLoopSetup
		# provided additional setup required before exectuing the test loop
		return false unless super
		self.removeDirectory(@dataDirPath) unless self.debugMode
		@savedAllowNet = @saCommand.readSetting("allowNetwork", "no")
		@savedDataDir = @saCommand.readSetting("databaseLocation", "/var/mysql")
		return true
	end
	
	#----------------------------------------
	def doTestLoopCleanup
		# provided additional cleanup required after the test loop is finished
		STDOUT.puts ""
		self.logAndShow("END OF TESTS")
		self.logAndShow("- restoring MySQL default settings")
		self.doRestoreSettings
		return super
	end

	#----------------------------------------
	def doRestoreSettings
		# called from RestoreSettingsCommand.doToolCommand
		# restore default settings for MySQL service
		self.logAndShow("- stopping MySQL service")
		@saCommand.stopService
		self.restoreDefaultSetting("allowNetwork", @savedAllowNet)
		self.restoreDefaultSetting("databaseLocation", "\"/var/mysql\"")
		# MySQL must be running to set the password
		#self.logMessage("- starting MySQL service")
		#@saCommand.startService
		#self.logMessage("  restoring default setting: rootPassword")
		#self.resetRootPassword
		#@saCommand.stopService
		return true
	end

end

#------------------------------------------------------------
# Custom command classes for this service
#------------------------------------------------------------

class MySQLCheckDatabaseCommand < QLCommand
	def initialize(qltool)
		dp = qltool.dataDirPath
		cmd = "ls \"#{dp}/ib_logfile0\""
		super(qltool, cmd, "#{dp}/ib_logfile0", MANUAL_REDIRECT)
	end
end

class MySQLCheckVarsCommand < QLCommand
	def initialize(qltool, key, value)
		cmd = "#{MySQLTool::MA_VARS_CMD} | grep #{key} | awk '{print $4}'"
		super(qltool, cmd, value, MANUAL_REDIRECT)
	end
end

class MySQLQueryUserCommand < QLCommand
	def initialize(qltool, user)
		cmd = "#{MySQLTool::QUERY_USER__PWD_CMD} | grep #{user} | grep localhost | awk '{print $2}'"
		super(qltool, cmd, user, MANUAL_REDIRECT)
	end
end

class MySQLResetPasswordCommand < QLCommand
	def initialize(qltool)
		cmd = "#{MySQLTool::RESET_PWD_CMD}"
		super(qltool, cmd, "", MANUAL_REDIRECT)
	end
end

#------------------------------------------------------------
# Custom test case classes for this service
#------------------------------------------------------------

class StopService < QLTestCase
	def initialize(seq, qltool)
		qlcmd = SAStateCommand.new(qltool, SAStateCommand::STATE_STOP)
		super(seq, qltool, "Basic", "Stop MySQL service", qlcmd)
	end
end

class StartService < QLTestCase
	def initialize(seq, qltool)
		qlcmd = SAStateCommand.new(qltool, SAStateCommand::STATE_START)
		super(seq, qltool, "Basic", "Start MySQL service", qlcmd)
	end
end

class SetDatabaseLocation < QLTestCase
	def initialize(seq, qltool)
		dd = qltool.dataDirPath
		qlcmd = SASettingsCommand.new(qltool, "databaseLocation", "\"#{dd}\"")
		super(seq, qltool, "Database", "Set MySQL active database location", qlcmd)
	end
end

class VerifyDatabaseLocation < QLTestCase
	def initialize(seq, qltool)
		qlcmd = MySQLCheckDatabaseCommand.new(qltool)
		super(seq, qltool, "Database", "Verify MySQL active database location", qlcmd)
	end
end

class RestoreDatabaseLocation < QLTestCase
	def initialize(seq, qltool)
		qlcmd = SASettingsCommand.new(qltool, "databaseLocation", "\"/var/mysql\"")
		super(seq, qltool, "Database", "Restore default MySQL database location", qlcmd)
	end
end

class DisableAllowNetworking < QLTestCase
	def initialize(seq, qltool)
		qlcmd = SASettingsCommand.new(qltool, "allowNetwork", "no")
		super(seq, qltool, "Network", "Disable allow networking", qlcmd)
	end
end

class VerifyDisableAllowNetworking < QLTestCase
	def initialize(seq, qltool)
		qlcmd = MySQLCheckVarsCommand.new(qltool, "skip_networking", "ON")
		super(seq, qltool, "Network", "Verify allow networking disabled", qlcmd)
	end
end

class EnableAllowNetworking < QLTestCase
	def initialize(seq, qltool)
		qlcmd = SASettingsCommand.new(qltool, "allowNetwork", "yes")
		super(seq, qltool, "Network", "Enable allow networking", qlcmd)
	end
end

class VerifyEnableAllowNetworking < QLTestCase
	def initialize(seq, qltool)
		qlcmd = MySQLCheckVarsCommand.new(qltool, "skip_networking", "OFF")
		super(seq, qltool, "Network", "Verify allow networking enabled", qlcmd)
	end
end

class RestoreAllowNetworking < QLTestCase
	def initialize(seq, qltool)
		qlcmd = SASettingsCommand.new(qltool, "allowNetwork", "no")
		super(seq, qltool, "Network", "Restore default allow networking setting", qlcmd)
	end
end

class SetRootPassword < QLTestCase
	def initialize(seq, qltool)
		qlcmd = SASettingsCommand.new(qltool, "rootPassword", "#{MySQLTool::TEST_PWD}", "\"\"")
		super(seq, qltool, "Password", "Set MySQL root password to \'admin\'", qlcmd)
	end
end

class VerifyRootPassword < QLTestCase
	def initialize(seq, qltool)
		qlcmd = MySQLQueryUserCommand.new(qltool, "root")
		super(seq, qltool, "Password", "Verify MySQL root password == \'admin\'", qlcmd)
	end
end

class RestoreRootPassword < QLTestCase
	# See MySQLTool::resetRootPassword for the command to restore the root password to blank.
	def initialize(seq, qltool)
		qlcmd = MySQLResetPasswordCommand.new(qltool)
		super(seq, qltool, "Password", "Restore default MySQL root password (blank)", qlcmd)
	end
end

#--------------------
# MAIN
#--------------------

	qltool = MySQLTool.new
	status = qltool.runCommand ? 0 : 1
	exit status

# END
