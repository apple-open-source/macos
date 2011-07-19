#!/usr/bin/ruby
if ARGV.length < 1 then
  $stderr.print "usage: filearch filename\n"
  exit(1)
end
file = ARGV[0]
IO.popen("/usr/bin/otool -hv #{file}", "r").grep(/MAGIC/) do |line|
  fields = line.split(/\s+/).delete_if {|f| f.empty?}
  if 0 == fields[2].casecmp("all")
    puts fields[1].downcase
  else
    puts fields[2]
  end
end
