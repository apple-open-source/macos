require 'osx/foundation'

include OSX

client = asl_open('RubyLog', 'RubyLog Facility', ASL_OPT_STDERR)

10.times do |i|
  asl_log(client, nil, ASL_LEVEL_NOTICE, "Message %d", i)
end

asl_close(client)
