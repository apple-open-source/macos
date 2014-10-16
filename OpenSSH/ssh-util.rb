#!/usr/bin/env ruby

ENV["PATH"] = "/bin:/usr/bin:/usr/sbin"

require 'optparse'
require 'cfpropertylist'

$dryRun = false
$debug = false
$verbose = false


$defaultsProg = "/usr/bin/defaults"
$plistBuddyProg = "/usr/libexec/PlistBuddy"
$remoteLoginStatusProg = "/usr/local/bin/remote-login-status"

#$sshdPlist = "/tmp/sshd.plist"
$sshdPlist = "/System/Library/LaunchDaemons/ssh.plist"
$sshdASLModule = "com.openssh.sshd"


module Logging
    NONE = 0
    STATUS = 1
    ENABLE = 2
    DISABLE = 3
end

def _doCommand (cmdLine)
    if $debug
        printf("-> %s\n", cmdLine)
    else
        cmdLine = cmdLine + " 2> /dev/null"
    end
    
    system(cmdLine) if !$dryRun
end

def _getStdOutputOfCommand (cmdLine)
    if $debug
        printf("-> %s\n", cmdLine)
    else
        cmdLine = cmdLine + " 2> /dev/null"
    end
    
    output = ""
    output = `#{cmdLine}` if !$dryRun
    
    output.chomp!
    
    printf("<- %s\n", output) if $debug
    
    return output
end

def _getAllOutputOfCommand (cmdLine)
    if $debug
	printf("-> %s\n", cmdLine)
    else
	cmdLine = cmdLine + " 2>&1"
    end
    
    output = ""
    output = `#{cmdLine}` if !$dryRun
    
    output.chomp!
    
    printf("<- %s\n", output) if $debug
    
    return output
end

def _getStatusOfCommand (cmdLine)
    if $debug
	printf("-> %s\n", cmdLine)
    else
	cmdLine = cmdLine + " 2> /dev/null"
    end
    
    output = ""
    output = `#{cmdLine}` if !$dryRun
    cmdStatus = $?

    output.chomp!
    
    printf("<- %s (%d)\n", output, cmdStatus) if $debug
    
    return cmdStatus
end


def SSHDLoggingEnable
    _doCommand("sudo #{$plistBuddyProg} -c \"add :ProgramArguments:2 string '-ddd'\" #{$sshdPlist}")
    _doCommand("sudo launchctl unload #{$sshdPlist}")
    _doCommand("sudo launchctl load #{$sshdPlist}")
    _doCommand("sudo touch /var/run/com.openssh.sshd-asl-enabled");
end

def SSHDLoggingDisable
    _doCommand("sudo #{$plistBuddyProg} -c \"Delete :ProgramArguments:2\" #{$sshdPlist}")
    _doCommand("sudo launchctl unload #{$sshdPlist}")
    _doCommand("sudo launchctl load #{$sshdPlist}")
    _doCommand("sudo rm -f /var/run/com.openssh.sshd-asl-enabled");
end

def SSHDLoggingStatus
    isEnabled = (_getStatusOfCommand("#{$plistBuddyProg} -c \"Print :ProgramArguments:2\" #{$sshdPlist}") == 0)
    printf("sshd logging is: %s\n", isEnabled ? "Enabled" : "Disabled")
end


def _checkFileMode(path, checkReadability=true)
    m = File.stat(path).mode & 0777
    
    printf("stat(#{path}): 0%o\n", m) if $debug
    
    if (m & 0022) != 0
	printf("%s is writable: 0%o\n", path, m)
        return false
    end
    
    if checkReadability && (m & 0044) != 0 && $verbose
	printf("%s is readable by group or other: 0%o; recommend it not be.\n", path, m)
    end
    
    return true
end
    
def diagnoseFilePermissions(homeDirPath)
    if _checkFileMode(homeDirPath, false)
        printf("%s: permissions OK\n", homeDirPath) if $verbose
    end
    
    filesToCheck = [".rhosts", ".shosts", ".ssh", ".ssh/environment", ".ssh/known_hosts", ".ssh/rc"]
    filesToCheck.each do |fileToCheck|
	file = File.join(homeDirPath, fileToCheck)
	if File.exists?(file) && _checkFileMode(file)
	    printf("#{file}: permissions OK.\n") if $verbose
	end
    end

    auth2 = File.join(homeDirPath, ".ssh", "authorized_keys2")
    if File.exists?(auth2)
	printf("%s: is not used by default\n", auth2);
    end
    
    auth = File.join(homeDirPath, ".ssh", "authorized_keys")
    if File.exists?(auth)
	if _checkFileMode(auth)
            printf("#{auth}: permissions OK\n") if $verbose
        end
    else
	printf("%s: does not exist.\n", auth);
    end

end

def diagnoseSharingPrefPaneStatus
    
    printf("Remote Login Sharing Preference Pane status: \n")
    
    if _getStatusOfCommand("#{$remoteLoginStatusProg} -q") == 0
        printf("\tsshd is DISABLED.")
    else
        printf("\tsshd is enabled.")
    end
    printf("\n")
    
    # for reference, this file contains the status:
    #    /var/db/launchd.db/com.apple.launchd/overrides.plist

end

# Service ACL for sshd is stored in the OD group com.apple.access_ssh
def diagnoseSACL
    printf("sshd SACL: \n")
    entryPlistString = _getAllOutputOfCommand("dscl -plist . read /Groups/com.apple.access_ssh")
    if entryPlistString =~ /DS Error/
        printf("   allows all users\n")
    else
        plist = CFPropertyList::List.new({:data => entryPlistString, :format => CFPropertyList::List::FORMAT_XML})
        entryPlist = CFPropertyList.native_types(plist.value)
        allowedUsers = entryPlist["dsAttrTypeStandard:GroupMembership"]
        if !allowedUsers.nil? && allowedUsers.length > 0
            printf("   allowed users: %s\n", allowedUsers.join(" "))
        end
        
        allowedGroups = entryPlist["dsAttrTypeStandard:NestedGroups"]
        if !allowedGroups.nil? && allowedGroups.length > 0
            printf("   allowed groups: %s\n", allowedGroups.join(" ")) if $debug
            printf("   allowed groups: ")
            allowedGroups.each do |groupUUID|
                output =_getAllOutputOfCommand("dscl /Search -search /Groups GeneratedUID #{groupUUID}")
                if !output.nil?
                    md = output.match(/^(.+)\t\t/)
                    if !md.nil? && !md[1].nil? && md[1].length > 0
                        printf("#{md[1]} ")
                        if $verbose && !ENV["USER"].nil?
                            user = ENV["USER"]
                            str = _getAllOutputOfCommand("dsmemberutil checkmembership -U #{user} -X #{groupUUID}")
                            #printf("\n '%s'\n", str)
                            if  str == "user is a member of the group"
                                printf("(#{user} is a member) ")
                            end
                        end
                    end
                end
            end
            printf("\n")
        end
    end
end

if __FILE__ == $0
    
    sshLogOption = Logging::NONE
    doDiag = false
    otherUserName = nil
    
    opts = OptionParser.new
    opts.banner = "usage: #{$0} [Options]"
    opts.on("-l", "--sshd-logging [LOG-OPTIONS]",         "sshd logging -- [enable|on, disable|off, status]")   { |val|
	sshLogOption = case val
	    when "enable", "on" , "1"
		 Logging::ENABLE
	    when "disable" , "off" , "0"
		 Logging::DISABLE
	    when "status"
		 Logging::STATUS
	    else
		puts opts
		exit(1)
	end
    }
    opts.on("-d", "--diagnose",	    "run diagnostics")		{|val| doDiag = val}
    opts.on("-u", "--for-user USER","run diagnostic for USER")	{|val| otherUserName = val}
    opts.on("-v", "--verbose",      "enable additional output")	{|val| $verbose = val}
    opts.on(      "--debug",	    "enable debugging output")	{|val| $debug = val}
    opts.on(      "--dryrun",	    "don't execute commands")	{|val| $dryRun = val}
    opts.on_tail("-h", "--help", "Show this message") do
        puts opts
        exit
    end
    
    
    begin
        # parse ARGV in order and terminate processing on the first unregonized arg
        #otherArgs = opts.order(ARGV) {|val| raise OptionParser::ParseError, val}
        otherArgs = opts.order(ARGV)
    rescue OptionParser::ParseError => error
        puts error.message
        puts opts
        exit
    end
    

    if sshLogOption != Logging::NONE
	case sshLogOption
	    when Logging::ENABLE
		SSHDLoggingEnable()
	    when Logging::DISABLE
		SSHDLoggingDisable()
	    when Logging::STATUS
		SSHDLoggingStatus()
	end
    end
    
    if doDiag
	homeDirectoryPath = otherUserName.nil? ? ENV["HOME"] : Etc.getpwnam(otherUserName).dir

	diagnoseFilePermissions(homeDirectoryPath)
	printf("\n")
	diagnoseSharingPrefPaneStatus()
	printf("\n")
	diagnoseSACL()
    end
    
end
