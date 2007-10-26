require 'test/unit'

`ls tc_*.rb`.each do |testcase|
  testcase.chop!
  $stderr.puts testcase if $VERBOSE
  require( testcase )
end
