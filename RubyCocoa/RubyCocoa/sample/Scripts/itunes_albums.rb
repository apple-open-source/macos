require 'osx/cocoa'
require 'kconv'

class AppleScript
  include OSX

  def initialize (src, raise_err_p = true)
    @script = NSAppleScript.alloc.initWithSource(src)
    @errinfo = OCObject.new
    @script.compileAndReturnError?(@errinfo)
    @script = nil if handle_error(@errinfo, raise_err_p)
  end

  def execute (raise_err_p = false)
    @errinfo = OCObject.new
    result = @script.executeAndReturnError(@errinfo)
    handle_error(@errinfo, raise_err_p)
    return result
  end

  def source
    @script.source.to_s
  end

  def error?
    return nil if @errinfo.ocnil?
    return (errmsg_of @errinfo)
  end

  private

  def handle_error (errinfo, raise_err_p)
    return false if errinfo.ocnil?
    if raise_err_p then
      raise "AppleScriptError: #{errmsg_of errinfo}"
    else
      $stderr.puts( errmsg_of(errinfo) )
    end
    return true
  end

  def errmsg_of (errinfo)
    errinfo.objectForKey('NSAppleScriptErrorMessage').to_s
  end

end

class AEList
  include Enumerable

  def initialize (aedesc)
    @aedesc = aedesc
  end

  def each
    @aedesc.numberOfItems.times do |index|
      yield @aedesc.descriptorAtIndex( index + 1 )
    end
    return self
  end

end

if __FILE__ == $0 then

  $stderr.puts "executing applescript ..."

  # AppleScript - all album names of iTunes playlist 1
  script = AppleScript.new %{
    tell application "iTunes"
    album of tracks of library playlist 1
    end tell
  }

  # execute and get result as AEList
  result = script.execute
  albums = AEList.new(result)

  # convert Ruby string and uniq
  albums = albums.map {|i| i.stringValue.to_s.toeuc }.uniq

  # print all alubum names
  albums.each do |title|
    puts title
  end

end
