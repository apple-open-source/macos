require 'osx/foundation'

unless ARGV.length == 2
  $stderr.puts "Usage: #{__FILE__} <source-file> <destination-file>"
  exit 1
end

code = OSX.copyfile(ARGV.first, ARGV.last, nil, OSX::COPYFILE_ALL)
if code < 0
  $stderr.puts "copyfile() error: #{code}"
  exit 1
end
