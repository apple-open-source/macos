#!/usr/bin/env ruby
# Make the computer speaking, using AppleScript.

class AppleScript
  require 'osx/cocoa'
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
    return errmsg_of(@errinfo)
  end

  private

  def handle_error (errinfo, raise_err_p)
    return false if errinfo.ocnil?
    if raise_err_p then
      raise "AppleScriptError: #{errmsg_of errinfo}"
    else
      $stderr.puts errmsg_of(errinfo)
    end
    return true
  end

  def errmsg_of (errinfo)
    errinfo.objectForKey('NSAppleScriptErrorMessage').to_s
  end

end

if __FILE__ == $0 then
  ARGF.each do |str|
    str.gsub!(/"/, '\"')
    src = %(say "#{str}")
    AppleScript.new(src).execute
  end
end
