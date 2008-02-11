#!/usr/bin/env ruby
# $Id: postflight-universal.rb 2030 2007-09-03 08:11:21Z psychs $
# install extlib "rubycocoa.bundle"

require 'rbconfig'
require 'fileutils'

srcfile = File.join('<%= @config['so-dir'] %>', 'rubycocoa.bundle')
destdir = Config::CONFIG['sitearchdir']
destfile = File.join(destdir, File.basename(srcfile))

begin

  if destfile != srcfile

    if File.exist? destfile
      puts "old extlib exists. remove:#{destfile}"
      File.unlink(destfile) 
    end

    unless File.exist? destdir
      puts "mkdir :#{destdir}"
      Dir.mkdir(destdir, 0755) 
    end

    puts "copy #{srcfile} to #{destfile}"
    File.link srcfile, destfile

  end

  libruby = '/usr/lib/libruby.1.dylib'
  patched_libruby = '/usr/lib/libruby-with-thread-hooks.1.dylib'
  if File.exist?(libruby) and File.exist?(patched_libruby)
    unless File.symlink?(libruby)
      puts "Creating a backup of #{libruby}"
      FileUtils.mv libruby, '/usr/lib/libruby.1.dylib.original'
      puts "Overwriting #{libruby} with #{patched_libruby}"
      File.symlink patched_libruby, libruby
    end
  else
    puts "Either libruby #{libruby} or patched libruby #{patched_libruby} doesn't exist, skipping overwrite..."
  end

  exit 0
rescue 
  $stderr.print <<EOS
##########################
postflight process failed!
##########################
Error: #{$!.inspect}
EOS
  exit 1
end
