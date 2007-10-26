#!/usr/bin/ruby
if ARGV.length < 1 then
  $stderr.print "usage: filearch filename\n"
  exit(1)
end
file = ARGV[0]
IO.popen("/usr/bin/otool -hv #{file}", "r").grep(/MAGIC/) do |line|
  fields = line.split(/\s+/)
  if 0 == fields[3].casecmp("all")
    print "#{fields[2].downcase}\n"
  else
    print "#{fields[3]}\n"
  end
end
