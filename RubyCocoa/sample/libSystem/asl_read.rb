require 'osx/foundation'

include OSX

client = asl_open('RubyLog', 'RubyLog Facility', ASL_OPT_STDERR)

q = asl_new(ASL_TYPE_QUERY)
asl_set_query(q, ASL_KEY_SENDER, 'RubyLog', ASL_QUERY_OP_EQUAL)
r = asl_search(client, q)

while m = aslresponse_next(r)
  i = 0
  while key = asl_key(m, i)
    puts "    #{key}: #{asl_get(m, key)}"
    i += 1
  end
end

aslresponse_free(r)
asl_free(q)
asl_close(client)
