###########################################
# Test Suite for Log::Log4perl::Level
# Mike Schilli, 2002 (m@perlmeister.com)
###########################################

#########################
# change 'tests => 1' to 'tests => last_test_to_print';
#########################
use Test;
use strict;

BEGIN { 
    if($ENV{INTERNAL_DEBUG}) {
        require Log::Log4perl::InternalDebug;
        Log::Log4perl::InternalDebug->enable();
    }
}

BEGIN { plan tests => 34 };
use Log::Log4perl::Level;
BEGIN {
    Log::Log4perl::Level->import("Level");
    Log::Log4perl::Level->import("My::Level");
}
ok(1); # If we made it this far, we're ok.

# Import them into the 'main' namespace;
foreach ($TRACE, $DEBUG, $INFO, $WARN, $ERROR, $FATAL) {
  ok(Log::Log4perl::Level::to_level($_));
}	

# Import them into the 'Level' namespace;
foreach ($Level::TRACE, $Level::DEBUG, $Level::INFO, $Level::WARN, $Level::ERROR, $Level::FATAL) {
  ok(Log::Log4perl::Level::to_level($_));
}

# Import them into the 'My::Level' namespace;
foreach ($My::Level::DEBUG, $My::Level::DEBUG, $My::Level::INFO, $My::Level::WARN, $My::Level::ERROR, $My::Level::FATAL) {
  ok(Log::Log4perl::Level::to_level($_));
}

# ok, now let's check to make sure the relative order is correct.

ok(Log::Log4perl::Level::isGreaterOrEqual($TRACE, $DEBUG));
ok(Log::Log4perl::Level::isGreaterOrEqual($DEBUG, $INFO));
ok(Log::Log4perl::Level::isGreaterOrEqual($INFO, $WARN));
ok(Log::Log4perl::Level::isGreaterOrEqual($WARN, $ERROR));
ok(Log::Log4perl::Level::isGreaterOrEqual($ERROR, $FATAL));

ok(Log::Log4perl::Level::isGreaterOrEqual($Level::TRACE, $Level::DEBUG));
ok(Log::Log4perl::Level::isGreaterOrEqual($Level::DEBUG, $Level::INFO));
ok(Log::Log4perl::Level::isGreaterOrEqual($Level::INFO, $Level::WARN));
ok(Log::Log4perl::Level::isGreaterOrEqual($Level::WARN, $Level::ERROR));
ok(Log::Log4perl::Level::isGreaterOrEqual($Level::ERROR, $Level::FATAL));

ok(Log::Log4perl::Level::isGreaterOrEqual($My::Level::TRACE, 
                                          $My::Level::DEBUG));
ok(Log::Log4perl::Level::isGreaterOrEqual($My::Level::DEBUG, $My::Level::INFO));
ok(Log::Log4perl::Level::isGreaterOrEqual($My::Level::INFO, $My::Level::WARN));
ok(Log::Log4perl::Level::isGreaterOrEqual($My::Level::WARN, $My::Level::ERROR));
ok(Log::Log4perl::Level::isGreaterOrEqual($My::Level::ERROR, $My::Level::FATAL));
