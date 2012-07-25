#!/usr/bin/ruby
if ARGV.length < 1 then
  $stderr.print "usage: version.rb project-name\n"
end

project = ARGV[0]
puts %Q(const char __#{project}_version[] = "@(#) #{project}-#{ENV['RC_ProjectSourceVersion']}";)

