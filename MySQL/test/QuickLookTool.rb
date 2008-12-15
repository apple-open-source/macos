# Name:        QuickLookTool.rb
# Author:      Steve Peralta
# Date:        9/16/07
# Description: QuickLookTest and related classes

$kQuickLookToolVers = "1.7"

# -------------------------------------------------
# To create a test tool based on QuickLookTool:
# -------------------------------------------------
# 1. Define a subclass of QuickLookTest
#
#     -- at minimum:
#
#          * In initialize(), call setTestCases() passing an array of test 
#            classes specific to your test program. For example:
#                class MyServiceTool < QuickLookTool
#                    def initialize
#                       super("myservice_quicklook", "1.0", "MyService", "myserviced", "MyService Quicklook Test Suite")
#                       self.setTestCases([TestCase1, TestCase2, ...])
#                       ...
#     -- optionally:
#
#          * In initialize(), call setPrereqFiles() with an array of files 
#            that must exist for the selected tests to be performed. For example:
#                def initialize
#                    ...
#                    self.setPrereqFiles(["/usr/libexec/myserviced", "/usr/bin/myservicetool"])
#                    ...
#
#          * Override doTestLoopSetup to perform actions prior to test-loop execution
#
#          * Override doTestLoopCleanup to perform actions after test-loop execution.
#
#          * Override canRun? to verify non-file prerequisites that would prevent
#            your test from running.  If your prerequisites consist only of files, 
#            then use setPrereqFiles() as outlined above.
#
# 2. Define sub-classes of QLTestCase for each individual test case
#
#     -- For tests that consist of arbitrary shell commands, subclass QLTestCase and 
#        initialize with a standard QLCommand instance. For example:
#            class TestCase1 < QLTestCase
#            	def initialize(seq, qltool)
#                   qlcmd = QLCommand.new(qltool, "/usr/bin/sometest --dothis --dothat", "did it!")
#            		super(seq, qltool, "Standard", "Do some test", qlcmd)
#                   ...
#
#     -- For tests based on serveradmin commands, subclass QLTestCase and initialize with 
#        an instance of SACommand.  Typically, tests of this type will create in instance
#        of the more specialize SACommand sub-class SAStateCommand or SASettingsCommand.
#        For example:
#            class StartServiceTest < SATestCase
#            	def initialize(seq, qltool)
#                   qlcmd = SAStateCommand.new(qltool, SAStateCommand::STATE_START)
#            		super(seq, qltool, "Standard", "Start MyExample service", qlcmd)
#                   ...
#            class SetFoobarSizeTest < SATestCase
#            	def initialize(seq, qltool)
#                   qlcmd = SASettingsCommand.new(qltool, "FoobarKey", "777")
#            		super(seq, qltool, "Standard", "Set Foobar to default value", qlcmd)
#                   ...
#
#     -- If your test is too complicated to fit in a single shell command, consider
#        generating an external custom script and letting doTest run that as the command.
#        Your script can send text to STDOUT that can be matched with the "expected" text.
#
#     -- If all else fails, override QLTestCase::doTest, but don't forget to support
#        the XILog hooks in your override method.
#
# ----------------------------------------
# To start your the test tool (MAIN):
# ----------------------------------------
#
# 1. create an instance of your QuickLookTest class
# 3. call my_class.runCommand and exit with the failed/success status
# 4. that's all!
#
# Example MAIN:
#    qltool = MyServiceTool.new
#    status = qltool.runCommand ? 0 : 1
#    exit status
#
# ----------------------------------------
# Running your tool
# ----------------------------------------
#
# The QuickLookTool class supports a number of built-in options.
# Run 'myservice_quicklook --help' for a list of commands and options 
# supported by the QuickLookTool class or browse the parseOptions 
# method for details.
#

#------------------------------------------------------------
# QuickLookTool 
#
# Abstract base class implementation for any Server 
# Admin-based quicklook test.  QuickLookTool manages 
# quicklook test files, logging, error messages, and 
# other common utility methods.
#------------------------------------------------------------

class QuickLookTool

	TOOL_VERSION = $kQuickLookToolVers

	BANNER_BORDER = "============================================================"
	
	#----------------------------------------
	def initialize(toolName, toolVersion, serviceDesc, serviceID, startBanner)
		@toolName = toolName
		@toolVersion = toolVersion
		@serviceDesc = serviceDesc
		@serviceID = serviceID
		
		@tempDir = "/tmp/#{@toolName}"
		@testLogPath = "#{@tempDir}/#{@toolName}.log"

		@prereqFiles = nil
		@testCases = nil

		@startBanner = startBanner
		@testLog = nil
		@debugMode = false

		@toolCommand = nil
	end
	
	#----------------------------------------
	def toolName
		return @toolName
	end
	
	#----------------------------------------
	def serviceDesc
		return @serviceDesc
	end
	
	#----------------------------------------
	def serviceID
		return @serviceID
	end
	
	#----------------------------------------
	def tempDir
		return @tempDir
	end
	
	#----------------------------------------
	def testLogPath
		return @testLogPath
	end
	
	#----------------------------------------
	def debugMode
		return @debugMode
	end
	
	#----------------------------------------
	def toolCommand
		return @toolCommand
	end
	
	#----------------------------------------
	def toolVersion
		return @toolVersion
	end

	#----------------------------------------
	def prereqFiles
		return @prereqFiles
	end

	#----------------------------------------
	def setPrereqFiles(flist)
		@prereqFiles = flist
	end

	#----------------------------------------
	def testCases
		return @testCases
	end

	#----------------------------------------
	def setTestCases(tlist)
		@testCases = tlist
	end

	#----------------------------------------
	def isUserRoot?
		return true if self.debugMode
		my_uid = Process.uid
		return true if my_uid == 0
		STDERR.print "\n#{self.toolName} must be run as root! (current user ID: #{my_uid})\n\n"
		return false
	end

	#----------------------------------------
	def getTime
		`date '+%Y/%m/%d %H:%M:%S'`.chomp!
	end
	
	#----------------------------------------
	def createLogFile
		return @testLog = File.open(@testLogPath, "a") if File.exists?(@testLogPath)

		STDOUT.puts "creating log file"
		@testLog = File.new(@testLogPath, "a", 0666)
		self.logMessage("")
		self.logMessage("******")
		self.logMessage("")
		self.logMessage("created new log")
		return @testLog
	end
	
	#----------------------------------------
	def createTestDir
		return if File.exists?(@tempDir)
		Dir.mkdir(@tempDir, 0777)
		if !File.exists?(@tempDir) then
			self.showError("Failed to create quicklook test directory: #{@tempDir}")
			exit
		end
	end

	#----------------------------------------
	def removeDirectory(dPath)
		`rm -rf #{dPath}` if File.exists?(dPath)
	end

	#----------------------------------------
	def removeFiles(fList)
		fList.each do |aFile|
			File.delete(aFile) if File.exists?(aFile)
		end
	end
	
	#----------------------------------------
	def readFromFile(fpath)
		return nil if !File.exists?(fpath)
		return File.read(fpath).chomp
	end
	
	#----------------------------------------
	def writeToFile(fpath, stringVal)
		File.delete(fpath) if File.exists?(fpath)
		f = File.new(fpath, "w")
		if stringVal.length > 0
			f.puts(stringVal)
		else
			f.print(stringVal)
		end
		f.close
	end

	#----------------------------------------
	def copyFileToConsole(fpath)
		data = self.readFromFile(fpath)
		STDOUT.puts(data)
	end
	
	#----------------------------------------
	def showError(aMsg)
		errMsg = "** Error -- #{aMsg}"
		STDOUT.puts ""
		STDOUT.puts errMsg
		self.logMessage(errMsg)
	end

	#----------------------------------------
	def logMessage(aMsg)
		return if @testLog == nil
		@testLog.puts "#{self.getTime} #{aMsg}"
	end
	
	#----------------------------------------
	def logAndShow(aMsg)
		STDOUT.puts aMsg
		self.logMessage(aMsg)
	end
	
	#----------------------------------------
	def logAndShowItems(aMsg, items)
		outMsg = aMsg
		sep = items.size > 1 ? ', ' : ''
		items.each_index do |i|
			outMsg = "#{outMsg}#{sep}" if i > 0
			outMsg = "#{outMsg}#{items[i]}"
		end
		STDOUT.puts outMsg
		self.logMessage(outMsg)
	end

	#----------------------------------------
	def logFileData(fpath)
		return if !File.exists?(fpath)
		data = File.read(fpath).chomp
		self.logMessage(data)
	end

	#----------------------------------------
	def getHardwareProfile
		hwprofile = Hash.new
		sp_out = `system_profiler SPHardwareDataType`.chomp
		sp_items = sp_out.split("\n")
		sp_items.each_index do |i|
			it = sp_items[i].split(": ")
			next if it == nil || it[0] == nil || it[1] == nil || it[0].length < 1
			key = it[0].strip
			value = it[1].strip
			hwprofile[key] = value
		end
		return hwprofile
	end

	#----------------------------------------
	def getHardwareProfileDescription
		hwp = getHardwareProfile
		return "\<no hardware profile\>" if hwp.length < 1
		mname = hwp["Model Name"]
		mid = hwp["Model Identifier"]
		pname = hwp["Processor Name"]
		pspeed = hwp["Processor Speed"]
		ncores = hwp["Total Number Of Cores"]
		nL2 = hwp["L2 Cache (per processor)"]
		mem = hwp["Memory"]
		bspeed = hwp["Bus Speed"]
		rvers = hwp["Boot ROM Version"]
		svers = hwp["SMC Version"]
		snum = hwp["Serial Number"]
		return "#{mname} (#{pname}) #{pspeed}; #{mem} RAM"
	end
	
	#----------------------------------------
	def parseOptions
	 	#
		# Parse command line options and set up for runCommand
		#
		require 'optparse'
		require 'optparse/time'
		require 'ostruct'
		
		options = OpenStruct.new
		options.vers = false
		options.help = false
		options.helptext = ""
		options.list = false
		options.tests = ""
		options.groups = ""
		options.xilog = false
		options.xiowner = ""
		options.xiconfig = ""
		options.install = false
		options.restore = false
		options.debug = false

		OptionParser.new do |opts|
			opts.banner = "Usage: #{@toolName} [options]"

	        opts.on_head("-h", "--help", "Show this message") do |he|
				options.help = he
				options.helptext = opts
	        end
			opts.on("-l", "--list", "List available test cases") do |lt|
				options.list = lt
			end
			opts.on("-g:", "--groups", "Run only test cases from selected group(s) [all]") do |tg|
				options.groups << tg
			end
			opts.on("-t:", "--tests", "Run only selected test case(s) [all]") do |tc|
				options.tests << tc
			end
			opts.on("-x", "--[no-]xilog", "Enable XILog [disabled]") do |xl|
				options.xilog = xl
			end
			opts.on("-o:", "--owner", "XILog Test Owner [required for --xilog option]") do |xo|
				options.xiowner << xo
			end
			opts.on("-c:", "--config", "Test configuration description") do |cf|
				options.xiconfig << cf
			end
			opts.on("-i", "--install", "Install RubyCocoa-XILog bridge file") do |ib|
				options.install = ib
			end
			opts.on("-r", "--restore", "Restore service default settings") do |rs|
				options.restore = rs
			end
			opts.on("-d", "--[no-]debug", "Run simulated test for script debugging [disabled]") do |db|
				options.debug = db
			end
	        opts.on_tail("-v", "--version", "Show tool version") do |ve|
				options.vers = ve
	        end
		end.parse!

		@debugMode = options.debug
		
		@toolCommand = ShowHelpCommand.new(self, options.helptext, @startBanner) if options.help
		@toolCommand = ShowVersionCommand.new(self, @startBanner) if options.vers
		@toolCommand = InstallBridgeCommand.new(self, "XILog") if options.install
		@toolCommand = RestoreSettingsCommand.new(self) if options.restore
		return if @toolCommand != nil

		selTests = nil
		selGroups = options.groups.length > 0 ? options.groups.split(',') : nil
		if selGroups == nil then
			selTests = options.tests.length > 0 ? options.tests.split(',') : nil
		end
		@toolCommand = ShowListCommand.new(self, selGroups, selTests) if options.list
		return if @toolCommand != nil

		xilogger = nil
		if options.xilog then
			if options.xiowner.length < 1 then
				STDOUT.print "\nEnter XILog tool owner (ex: com.apple.myteam): "
				options.xiowner = STDIN.gets.chomp
				if options.xiowner.length < 1 then
					self.showError("Missing XILog tool owner (REQUIRED)")
					@toolCommand = ShowHelpCommand.new(self, nil, @startBanner)
					return 
				end
			end
			if options.xiconfig.length < 1 then
				STDOUT.print "Enter XILog configuration [optional]: "
				options.xiconfig = STDIN.gets.chomp
				if options.xiconfig.length < 1 then
					options.xiconfig = self.getHardwareProfileDescription
				end
			end
			xilogger = XILogger.new(options.xiowner, options.xiconfig, self)
		end

		@toolCommand = RunTestLoopCommand.new(self, xilogger, selGroups, selTests, @startBanner)

	end

	#----------------------------------------
	def runCommand
		self.parseOptions
		if @toolCommand == nil then
			self.showError()
		end
		STDOUT.puts ""
		return false unless @toolCommand.canRun?
		return @toolCommand.doToolCommand
	end

	#----------------------------------------
	def doTestLoopSetup
		# called from RunTestLoopCommand.doToolCommand
		# override this method to perform any pre-test-loop actions
		return File.exists?(@tempDir)
	end

	#----------------------------------------
	def doTestLoopCleanup
		# called from RunTestLoopCommand.doToolCommand
		# override this method for special post-test actions
		return true
	end

	#----------------------------------------
	def doRestoreSettings
		# called from RestoreSettingsCommand.doToolCommand
		# override this method to restore default
		# settings for this service
		return true
	end

	#----------------------------------------
	def debugMsg(aMsg, methName)
		STDOUT.puts "DEBUG: #{aMsg} \<#{self.class.name}.#{methName}\>" if @debugMode
	end

end

#------------------------------------------------------------
# QLToolCommand - abstract class for top-level commands
#
# This class defines the purpose of the current invocation
# of QuickLookTool based on the options and arguments 
# specified in the command line.
#
# instance variables
#   debugMode   true = run tool with simulated responses
#                      (no actual commands are executed)
#------------------------------------------------------------
class QLToolCommand

	#----------------------------------------
	def initialize(qltool)
		@qltool = qltool
	end

	#----------------------------------------
	def qltool
		return @qltool
	end

	#----------------------------------------
	def canRun?
		return true
	end

	#----------------------------------------
	def doToolCommand
		# based class provided no default implementation,
		# derived classes must override
		return true
	end

	#----------------------------------------
	def debugMsg(aMsg, methName)
		STDOUT.puts "DEBUG: #{aMsg} \<#{self.class.name}.#{methName}\>" if @qltool.debugMode
	end
	
end

#------------------------------------------------------------
# ShowVersionCommand - display the program version info
#------------------------------------------------------------
class ShowVersionCommand < QLToolCommand

	#----------------------------------------
	def initialize(qltool, versBanner)
		super(qltool)
		@versBanner = versBanner
	end

	#----------------------------------------
	def doToolCommand
		STDOUT.puts "#{@versBanner} v#{@qltool.toolVersion} " + 
		            "(QuickLookTool v#{QuickLookTool::TOOL_VERSION})"
		STDOUT.puts ""
		return true
	end

end

#------------------------------------------------------------
# ShowHelpCommand - display the program help text
#------------------------------------------------------------
class ShowHelpCommand < ShowVersionCommand

	#----------------------------------------
	def initialize(qltool, helpText, versBanner)
		super(qltool, versBanner)
		@helpText = helpText
	end

	#----------------------------------------
	def doToolCommand
		super unless @helpText == nil
		STDOUT.puts @helpText unless @helpText == nil
		STDOUT.puts "Use '#{@qltool.toolName} --help' to see" +
		                   " all available options." if @helpText == nil
		return true
	end
	
end

#------------------------------------------------------------
# ShowListCommand - displays the list of available test cases
#------------------------------------------------------------
class ShowListCommand < QLToolCommand

	#----------------------------------------
	def initialize(qltool, selGroups, selTests)
		super(qltool)
		@selGroups = selGroups
		@selTests = selTests
	end

	#----------------------------------------
	def doToolCommand
		STDOUT.puts "Test cases:"
		STDOUT.puts ""
		cases = self.selectTestCases
		if cases.size < 1 then
			STDOUT.puts "No matching test cases found."
		else
			format = "%-12s %-32s %s\n"
			STDOUT.printf format, "Group", "Test Name", "Description"
			STDOUT.printf format, "-----", "---------", "-----------"
			cases.each do |tc|
				aTest = tc.new(0, @qltool)
				ti = aTest.getInfo
				STDOUT.printf format, ti[0], ti[1], ti[2]
			end
		end
		return true
	end
	
	#----------------------------------------
	def selectTestCases
		# Creates filtered list of tests based on the selected 
		# groups or selected tests command line options
		selCases = []
		if @selGroups != nil || @selTests != nil then
			@qltool.logAndShowItems("* Selected test group(s): ", @selGroups) if @selGroups != nil
			@qltool.logAndShowItems("* Selected test case(s): ", @selTests) if @selTests != nil
			@qltool.testCases.each do |tc|
				aTest = tc.new(0, qltool)
				selected = false
				if @selGroups != nil then
					@selGroups.each do |sg|
						selected = sg == aTest.getGroup
						break if selected
					end
				elsif @selTests != nil
					@selTests.each do |st|
						selected = st == aTest.class.name 
						break if selected
					end
				else
					selected = true
				end
				next unless selected
				selCases << tc
			end
		else
			STDOUT.puts "* Selected test cases: \<ALL\>"
			selCases = @qltool.testCases
		end
		return selCases
	end
	
end

#------------------------------------------------------------
# RestoreSettingsCommand - restores the service default settings
#------------------------------------------------------------
class RestoreSettingsCommand < QLToolCommand

	#----------------------------------------
	def canRun?
		return @qltool.isUserRoot?
	end

	#----------------------------------------
	def doToolCommand
		@qltool.logAndShow("Restoring default settings for #{@qltool.serviceDesc}")
		return @qltool.doRestoreSettings
	end
end

#------------------------------------------------------------
# InstallBridgeCommand - installs the Ruby bridgesupport 
#                        for the selected framework
#------------------------------------------------------------
class InstallBridgeCommand < QLToolCommand

	#----------------------------------------
	def initialize(qltool, fwname)
		super(qltool)
		@fwname = fwname
	end

	#----------------------------------------
	def canRun?
		return @qltool.isUserRoot?
	end

	#----------------------------------------
	def doToolCommand
		fw="#{@fwname}.framework"
		STDOUT.puts "Checking for required #{fw}"
		self.debugMsg("skipping installation", "doToolCommand")
		STDOUT.puts ""
		return true if @qltool.debugMode

		bsFile="#{@fwname}.bridgesupport"
		bsDir="BridgeSupport"

		fwBase="/AppleInternal/Library/Frameworks"
		fwPath="#{fwBase}/#{fw}"

		fwPathRel="#{fw}/Resources"
		bdPathRel="#{fwPathRel}/BridgeSupport"
		bfPathRel="#{bdPathRel}/#{bsFile}"

		fwPathAbs="#{fwBase}/#{fwPathRel}"
		bdPathAbs="#{fwBase}/#{bdPathRel}"
		bfPathAbs="#{fwBase}/#{bfPathRel}"

		if !File.exists?("#{fwPath}") then
			STDOUT.puts "#{fwname}.framework not installed, exiting"
			return
		end
		if File.exists?("#{bfPathAbs}") then
			STDOUT.puts "#{fw} bridge support already installed, no action taken"
			return false
		end
		STDOUT.puts "Processing #{fw}.."
		if !File.exists?("#{bdPathAbs}") then
			STDOUT.puts "..creating directory: ../#{bdPathRel}"
			mkdirCmd = "/bin/mkdir -p \"#{bdPathAbs}\""
			if @qltool.debugMode then
				self.debugMsg("cmd = \"#{mkdirCmd}\"", "doToolCommand")
			else
				`#{mkdirCmd}`
			end
			if !@qltool.debugMode && !File.exists?("#{bdPathAbs}") then
				STDOUT.puts "\n\n** Error -- failed to create \"#{bdPathAbs}\", exiting"
				return false
			end
		end
		STDOUT.puts "..creating metadata: ../#{bfPathRel}"
		genCmd = "/usr/bin/gen_bridge_metadata --framework \"#{fwPath}\" --output \"#{bfPathAbs}\""
		if @qltool.debugMode then
			self.debugMsg("cmd = \"#{genCmd}\"", "doToolCommand")
		else
			`#{genCmd}`
		end
		if !@qltool.debugMode && !File.exists?("#{bfPathAbs}") then
			STDOUT.puts "** Error -- #{fw} bridge support installation failed."
		else
			STDOUT.puts "#{fw} bridge support successfully installed."
		end
		return true
	end
	
end

#------------------------------------------------------------
# RunTestLoopCommand - executes the test loop for selected tests
#------------------------------------------------------------
class RunTestLoopCommand < ShowListCommand

	#----------------------------------------
	def initialize(qltool, xilogger, selGroups, selTests, startBanner)
		super(qltool, selGroups, selTests)
		@xilogger = xilogger
		
		@startBanner = startBanner
		
		@numEnabled = 0
		@numPassed = 0
	end

	#----------------------------------------
	def xilogger
		return @xilogger
	end

	#----------------------------------------
	def canRun?
		return false unless @qltool.isUserRoot?
		return false unless @xilogger != nil ? @xilogger.isInstalled? : true
		return true if @qltool.debugMode

		# verify optional test pre-req's
		prf = @qltool.prereqFiles
		return true if prf == nil
		prf.each do |fpath|
			next if File.exists?(fpath)
			@qltool.showError("#{fpath} not installed! -- aborting")
			return false
		end
		return true
	end

	#----------------------------------------
	def doToolCommand
		STDOUT.puts "\n * * * DEBUG MODE ENABLED (all tests simulated) * * *\n" if @qltool.debugMode

		@qltool.createTestDir
		@qltool.createLogFile

		self.showStartBanner
		@qltool.logAndShow("* XILog enabled    : #{@xilogger != nil ? "YES" : "NO"}")
		if @xilogger != nil then
			@qltool.logAndShow("* XILog tool owner : \"#{@xilogger.logOwner}\"")
			@qltool.logAndShow("* XILog config     : \"#{@xilogger.logConfig}\"")
		end
		@xilogger.doOpen unless @xilogger == nil

		# hook for derived class custom setup
		@qltool.logAndShow("* Initializing test environment")
		return false unless @qltool.doTestLoopSetup

		selCases = self.selectTestCases
		if selCases == nil || selCases.length < 1 then
			@qltool.logAndShow("No matching test cases for requests groups/tests.")
			return true
		end

		#
		# main test loop
		#
		@numEnabled = selCases.length
		seqNum = 0
		selCases.each do |testClass|
			seqNum += 1
			aTest = testClass.new(seqNum, qltool)
			if !aTest.doTest then
				@qltool.showError("one or more test failed!")
				break
			end
			@numPassed += 1
		end

		# hook for derived class custom cleanup
		testOk = @qltool.doTestLoopCleanup
		
		@xilogger.doClose unless @xilogger == nil

		self.showEndBanner

		return testOk
	end
	
	#----------------------------------------
	def showStartBanner
		@qltool.logAndShow("")
		@qltool.logAndShow(QuickLookTool::BANNER_BORDER)
		@qltool.logAndShow("#{@startBanner} v#{@qltool.toolVersion}" +
		                   " (QuickLookTool v#{QuickLookTool::TOOL_VERSION})")
		@qltool.logAndShow(QuickLookTool::BANNER_BORDER)
	end

	#----------------------------------------
	def showEndBanner
		percent = ( @numPassed * 100 ) / @numEnabled
		passedMsg = "Passed #{@numPassed} / #{@numEnabled} tests (#{percent}%)."

		endTitle = "ALL TESTS COMPLETED"

		# print summary to console and log
		STDOUT.puts ""
		@qltool.logAndShow(QuickLookTool::BANNER_BORDER)

		STDOUT.puts "#{@qltool.getTime} #{endTitle}"
		@qltool.logMessage(endTitle)

		@qltool.logAndShow(passedMsg)
		STDOUT.puts ""
		STDOUT.puts "All test results logged in: #{@qltool.testLogPath}"
		@qltool.logAndShow("XILog output file: #{@xilogger.logOutPath}") unless @xilogger == nil
		@qltool.logAndShow(QuickLookTool::BANNER_BORDER)
	end
	
end

#------------------------------------------------------------
# QLTestCase - abstract base class for quicklook test cases
#
# QLTestCase defines the methods and data for a single
# quicklook test case.
#
# instance variables
#   seq      test number
#   qltool   QuickLookTool instance for utility methods
#   desc     test description
#   qlcmd    QLCommand instance
#------------------------------------------------------------
class QLTestCase

	#----------------------------------------
	def initialize(seq, qltool, group, desc, qlcmd)
		@seq = seq
		@qltool = qltool
		@group = group
		@name = self.class.name
		@desc = desc
		@qlcmd = qlcmd
	end
	
	#----------------------------------------
	def getTool
		return @qltool
	end
	
	#----------------------------------------
	def getGroup
		return @group
	end
	
	#----------------------------------------
	def getInfo
		return [@group, @name, @desc]
	end
	
	#----------------------------------------
	def doTest
		testTitle = "[#{@seq}] #{@group}.#{@name}"
		testDesc = " [#{@seq}]:"
		descMsg = "TEST: #{testTitle}"
		STDOUT.print "\n#{@qltool.getTime} #{descMsg}"
		STDOUT.puts "" if @qltool.debugMode
		@qltool.logMessage("***")
		@qltool.logMessage(descMsg)

		@qltool.logMessage("#{testDesc} starting test")
		tc = @qltool.toolCommand
		xl = tc.xilogger
		xl.doBeginTest("#{@group}.#{@name}", @desc) unless xl == nil

		# execute the selected command
		@qlcmd.executeCommand(testDesc)

		# verify command results
		@qltool.logMessage("#{testDesc} checking results")
		testOk = @qlcmd.checkResults

		# log test results
		if testOk then
			STDOUT.puts " [PASSED]"
			@qltool.logMessage("#{testDesc} PASSED")
			#
			# report XILog success here
			#
		else
			# Log failed results
			border = "----------------------------------------"
			title = "RESULTS: epected vs. actual:"
			diffOut = @qlcmd.diffOutput

			# output results to the console
			STDOUT.puts(" [FAILED]", "")
			STDOUT.puts border
			STDOUT.puts title
			STDOUT.puts(diffOut)
			STDOUT.puts border
			STDOUT.puts ""

			# output results to the test log
			@qltool.logMessage("#{testDesc} FAILED")
			@qltool.logMessage(border)
			@qltool.logMessage(title)
			@qltool.logMessage(diffOut)
			@qltool.logMessage(border)
			#
			# report XILog failure here
			#
			xl.doLogError("TEST FAILED: \n#{title}\n#{diffOut}") unless xl == nil
		end

		xl.doEndTest(testOk) unless xl == nil

		return testOk
	end

	#----------------------------------------
	def debugMsg(aMsg, methName)
		STDOUT.puts "DEBUG: #{aMsg} \<#{self.class.name}.#{methName}\>" if @qltool.debugMode
	end
end

#------------------------------------------------------------
# QLCommand
#
# Class defining command parameters for QLTest
#------------------------------------------------------------
class QLCommand

	AUTO_REDIRECT = false
	MANUAL_REDIRECT = true

	#----------------------------------------
	# initialize args
	#   qltool    QuickLookTool reference
	#   command   shell command line
	#   expected  expected command output
	#   redir     true = command controls output redirection
	#             false = use standard output redirection
	def initialize(qltool, command, expected, redir = AUTO_REDIRECT)
		@qltool = qltool
		@command = command
		@expected = expected
		@redir = redir

		td = @qltool.tempDir
		@expectedOutPath = "#{td}/expected.out"
		@expectedErrPath = "#{td}/expected.err"
		@commandOutPath = "#{td}/command.out"
		@commandErrPath = "#{td}/command.err"
		@diffOutPath = "#{td}/diff.out"
	end

	#----------------------------------------
	def qltool
		return @qltool
	end

	#----------------------------------------
	def command
		return @command
	end

	#----------------------------------------
	def expected
		return @expected
	end

	#----------------------------------------
	def redir
		return @redir
	end

	#----------------------------------------
	def diffOutput
		data = @qltool.readFromFile(@diffOutPath)
		self.debugMsg("file not found: #{@diffOutPath}", "diffOutput") if data == nil
		return data != nil ? data : ""
	end

	#----------------------------------------
	def saveErrorOutputFiles
		system("cp", @expectedOutPath, @expectedErrPath)
		system("cp", @commandOutPath, @commandErrPath)
	end
	
	#----------------------------------------
	def executeCommand(desc)
		# remove any old results
		@qltool.removeFiles([ @expectedOutPath, @expectedErrPath, 
			                  @commandOutPath, @commandErrPath, 
			                  @diffOutPath ])

		# set the expected results
		@qltool.writeToFile(@expectedOutPath, @expected)

		# run the command, directing output to actual results
	    if qltool.debugMode then
			aMsg = "command(D) = \"#{@command}\""
			self.debugMsg(aMsg, "executeCommand")
			@qltool.logMessage("#{desc} #{aMsg}")
			@qltool.writeToFile(@commandOutPath, @expected)
		elsif @redir == AUTO_REDIRECT then
			# automatic output redirection
			@qltool.logMessage("#{desc} command(A) = \"#{@command}\"")
			cmdOut = `#{@command}`
			@qltool.writeToFile(@commandOutPath, cmdOut)
		else
			# manual output redirection
			cmd = "#{@command} > \"#{@commandOutPath}\""
			@qltool.logMessage("#{desc} command(M) = \"#{cmd}\"")
			system(cmd)
		end
	end

	#----------------------------------------
	def checkResults
		# Compare the command output with the expected 
		# output and record success or failure.  In case
		# of failure, the expected and actual output are
		# saved to prevent overwrite before the command
		# results are recorded in the log.
		`diff #{@expectedOutPath} #{@commandOutPath} > #{@diffOutPath}`
		testOk = File.size(@diffOutPath) == 0
		self.saveErrorOutputFiles unless testOk
		return testOk
	end

	#----------------------------------------
	def debugMsg(aMsg, methName)
		STDOUT.puts "DEBUG: #{aMsg} \<#{self.class.name}.#{methName}\>" if @qltool.debugMode
	end
end

#------------------------------------------------------------
# SACommand
#
# Specialized QLCommand sub-class for serveradmin commands
#------------------------------------------------------------
class SACommand < QLCommand

	SA_TOOL = "/usr/sbin/serveradmin"

	#----------------------------------------
	# initialize args
	#   qltool     QuickLookTool reference
	#   func       serveradmin function
	#   arg        function arguments if needed 
	#   results    expected output from command
	def initialize(qltool, func, args, results)
		cmd = "#{func} #{qltool.serviceID}"
		cmd = "#{cmd}:#{args}" if args != nil
		ev = results != nil ? results : ""
		super(qltool, "#{SA_TOOL} #{cmd}", "#{ev}")
	end
	
	#----------------------------------------
	def stopService
		# Stop the service outside the test enviroment
		self.setServiceState("stop")
	end
	
	#----------------------------------------
	def startService
		# Starting the service outside the test enviroment
		self.setServiceState("start")
	end

	#----------------------------------------
	def setServiceState(aVal)
		# Change the service state outside the test enviroment
		stateCmd = "#{SA_TOOL} #{aVal} #{@qltool.serviceID}"
		self.debugMsg("cmd = \"#{stateCmd}\"", "setServiceState")
		`#{stateCmd}` unless @qltool.debugMode
	end

	#----------------------------------------
	def readSetting(aKey, debugVal)
		# Read a service setting value outside the test enviroment
		rsCmd = "#{SA_TOOL} settings #{@qltool.serviceID} | grep #{aKey} | awk '{print $3}'"
		self.debugMsg("cmd = #{rsCmd}", "readSetting")
		return "#{@qltool.serviceID}:#{aKey} = #{debugVal}" if @qltool.debugMode
		return `#{rsCmd}`.chomp!
	end

	#----------------------------------------
	def writeSetting(aKey, aVal)
		# Write a service setting value outside the test enviroment
		wsCmd = "#{SA_TOOL} settings #{@qltool.serviceID}:#{aKey} = #{aVal}"
		self.debugMsg("cmd = \"#{wsCmd}\"", "writeSetting")
		`#{wsCmd}` unless @qltool.debugMode
	end
end

#------------------------------------------------------------
# SAStateCommand
#
# SACommand variant for setting service state
#------------------------------------------------------------
class SAStateCommand < SACommand

	STATE_STOP  = "stop"
	STATE_START = "start"
	
	#----------------------------------------
	# initialize args
	#   qltool     QuickLookTool reference
	#   state      new service state
	def initialize(qltool, state)
		ev = state == STATE_START ? "RUNNING" : "STOPPED"
		super(qltool, state, nil, "#{qltool.serviceID}:state = \"#{ev}\"")
	end
end

#------------------------------------------------------------
# SASettingsCommand
#
# SACommand variant for modifying service settings
#------------------------------------------------------------
class SASettingsCommand < SACommand

	#----------------------------------------
	# initialize args
	#   qltool     QuickLookTool reference
	#   key        settings key
	#   value      value to write for key
	#   expVal     if non-nil, use this value for 
	#              verifying the command result
	def initialize(qltool, key, value, expVal = nil)
		serviceID = qltool.serviceID
		ev = expVal != nil ? expVal : value
		super(qltool, "settings", "#{key} = #{value}", 
		      "#{serviceID}:#{key} = #{ev}")
	end

	#----------------------------------------
	def executeCommand(desc)
		# SASettingsCommand overrides the base class method 
		# to bracket command execution with a service recycle
		# in order to activate the new setting
		@qltool.logMessage("#{desc} stopping service: #{@qltool.serviceDesc}")
		self.stopService
		
		super(desc)
		
		@qltool.logMessage("#{desc} re-starting service: #{@qltool.serviceDesc}")
		self.startService
	end
end

#------------------------------------------------------------
# XILogger
#
# Wrapper class for XILog RubyCocoa bridge.  QuickLookTool
# creates an instance of this class if the --xilog option
# is selected.
#------------------------------------------------------------
class XILogger
	XILOG_XML = true
	XILOG_ECHO = false

	XILOG_FRAMEWORK     = "/AppleInternal/Library/Frameworks/XILog.framework"
	XILOG_BRIDGESUPPORT = "#{XILOG_FRAMEWORK}/Resources/BridgeSupport/XILog.bridgesupport"

	#----------------------------------------
	def initialize(owner, config, qltool)
		@logOwner = owner
		@logConfig = config

		@qltool = qltool

		@logOutPath = "#{qltool.tempDir}/xiLog.log"
		@logRef = nil

		require 'osx/cocoa' unless @qltool.debugMode
		#$: << "/AppleInternal/Library/Ruby"
		#require 'XILog'
	end

	#----------------------------------------
	def logOwner
		return @logOwner
	end

	def logConfig
		return @logConfig
	end

	def logOutPath
		return @logOutPath
	end

	#----------------------------------------
	def isInstalled?
		return true if @qltool.debugMode
		[XILOG_FRAMEWORK, XILOG_BRIDGESUPPORT].each do |fpath|
		#[XILOG_FRAMEWORK].each do |fpath|
			next if File.exists?(fpath)
			if fpath == XILOG_FRAMEWORK then
				@qltool.showError("XILog.framework is not installed.")
			else
				@qltool.showError("Missing BridgeSupport for XILog.framework.")
				STDOUT.puts "Hint: use \'#{@qltool.toolName} --install\' to create missing BridgeSupport file."
			end
			return false
		end
		return true
	end

	#----------------------------------------
	def doOpen
		if @qltool.debugMode then
			self.debugMsg("cmd = XILogOpenLog(\"#{@logOutPath}\", \"#{@qltool.toolName}\", \"#{@logOwner}\", \"#{@logConfig}\")", "doOpen")
			#self.debugMsg("cmd = XILog.open(\"#{@logOutPath}\", \"#{@qltool.toolName}\", \"#{@logOwner}\", \"#{@logConfig}\")", "doOpen")
			return
		end
		OSX.require_framework XILOG_FRAMEWORK
		@logRef = OSX.XILogOpenLog(@logOutPath, @qltool.toolName, @logOwner, @logConfig, XILOG_XML, XILOG_ECHO)
		#@logRef = XIlog::open(@logOutPath, @qltool.toolName, @logOwner, @logConfig, XILOG_XML, XILOG_ECHO)
		if @logRef == nil then
			errMsg = "WARNING: XILogOpenLog failed; continuing test with XILog disabled"
			@qltool.showError(errMsg)
			@qltool.logMessage(errMsg)
		end
	end

	#----------------------------------------
	def doClose
		if @qltool.debugMode then
			self.debugMsg("cmd = XILogCloseLog()", "doClose")
			#self.debugMsg("cmd = XILog.close", "doClose")
			return
		end
		OSX.XILogCloseLog(@logRef)
		#@logRef.close
	end

	#----------------------------------------
	def doBeginTest(name, desc)
		if @qltool.debugMode then
			self.debugMsg("cmd = XILogBeginTestCase(\"#{name}\", \"#{desc}\")", "doBeginTest")
			#self.debugMsg("cmd = XILog.begin_test(\"#{name}\", \"#{desc}\")", "doBeginTest")
			return
		end
		OSX.XILogBeginTestCase(@logRef, name, desc)
		#@logRef.begin_test(name, desc)
	end

	#----------------------------------------
	def doEndTest(testOk)
		if @qltool.debugMode then
			self.debugMsg("cmd = XILogEndTestCase(#{testOk.to_s})", "doEndTest")
			#self.debugMsg("cmd = XILog.end_test", "doEndTest")
			return
		end
		OSX.XILogEndTestCase(@logRef, testOk ? OSX::KXILogTestPass : OSX::KXILogTestFail)
		#@logRef.end_test
	end

	#----------------------------------------
	def doLogMessage(msg)
		if @qltool.debugMode then
			self.debugMsg("cmd = XILogMessage(\"#{msg})\"", "doLogMessage")
			#self.debugMsg("cmd = XILog.log(\"#{msg})\"", "doLogMessage")
			return
		end
		OSX.XILogMessage(@logRef, msg)
		#@logRef.log(msg)
	end

	#----------------------------------------
	def doLogError(msg)
		if @qltool.debugMode then
			self.debugMsg("cmd = XILogError(\"#{msg}\")", "doLogError")
			#self.debugMsg("cmd = XILog.error(\"#{msg}\", XILog::ERROR)", "doLogMessage")
			return
		end
		OSX.XILogError(@logRef, msg)
		#@logRef.error(msg, XILog::ERROR)
	end

	#----------------------------------------
	def debugMsg(aMsg, methName)
		STDOUT.puts "\nDEBUG: #{aMsg} \<#{self.class.name}.#{methName}\>" if @qltool.debugMode
	end
end
