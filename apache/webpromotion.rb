#!/System/Library/PrivateFrameworks/MacRuby.framework/Versions/Current/usr/bin/macruby

# Copyright (c) 2010-2011 Apple Inc.  All rights reserved.

class WebPromotion
  def initialize
    @launchKey = "org.apache.httpd"
    @plistFile = "/System/Library/LaunchDaemons/#{@launchKey}.plist"
    @plistDict = NSDictionary.dictionaryWithContentsOfFile(@plistFile)
    @args = @plistDict["ProgramArguments"]
  end
  def apacheIsRunning
    `/bin/launchctl list #{@launchKey} >/dev/null 2>&1`
    return $?.exitstatus == 0
  end
  def promote
    if statusString != "SERVER"
      @args <<"-D"<<"MACOSXSERVER"
      @plistDict.writeToFile(@plistFile, atomically:true)
      `/usr/sbin/apachectl restart` if apacheIsRunning 
    end
  end
  def demote
    if statusString != "DESKTOP"
      index = @args.index("MACOSXSERVER")
      if !index.nil?
        @args.delete_at(index)
        @args.delete_at(index - 1)
      end
      @plistDict.writeToFile(@plistFile, atomically:true)
      `/usr/sbin/apachectl restart` if apacheIsRunning 
    end
  end
  def status
    $stdout.print("#{statusString}\n")
  end
  def statusString
    if @args.join(' ').include?("-D MACOSXSERVER") 
      return "SERVER"
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

