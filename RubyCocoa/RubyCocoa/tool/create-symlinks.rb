# This script backups an existing RubyCocoa installation and creates symlinks
# to the newly built RubyCocoa, in the current directory.
#
# Usage (in the RubyCocoa project root directory):
#    $ sudo ruby tool/create-symlinks.rb

require 'fileutils'
include FileUtils

unless File.exist?('tool/create-symlinks.rb')
  $stderr.puts 'This tool should be used in the RubyCocoa project root directory'
  exit 1
end

puts 'Are you sure to backup the existing RubyCocoa installation and create symlinks to this new one instead? (y/N)'
exit unless gets.chomp == 'y'
puts 'Really? (y/N)'
exit unless gets.chomp == 'y'

pwd = Dir.pwd

Dir.chdir('/System/Library/Frameworks') {
  mv('RubyCocoa.framework', 'RubyCocoa.framework.old')
  ln_s(File.join(pwd, 'framework/build/Default/RubyCocoa.framework'), 'RubyCocoa.framework')  
}

Dir.chdir('/usr/lib/ruby/1.8') {
  mv('osx', 'osx.old')
  ln_s(File.join(pwd, 'lib/osx'), 'osx')
  Dir.chdir('universal-darwin9.0') {
    mv('rubycocoa.bundle', 'rubycocoa.bundle.old')
    ln_s(File.join(pwd, 'ext/rubycocoa/rubycocoa.bundle'), 'rubycocoa.bundle')
  }
}
