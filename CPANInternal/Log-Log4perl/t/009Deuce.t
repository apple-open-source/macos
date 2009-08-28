###########################################
# Test Suite for Log::Log4perl
# Test two appenders in one category
# Mike Schilli, 2002 (m@perlmeister.com)
###########################################

#########################
# change 'tests => 1' to 'tests => last_test_to_print';
#########################
use Test;
BEGIN { plan tests => 5 };

use Log::Log4perl;
use Log::Log4perl::Appender::TestBuffer;

my $EG_DIR = "eg";
$EG_DIR = "../eg" unless -d $EG_DIR;

ok(1); # If we made it this far, we're ok.

######################################################################
# Test the root logger on a configuration file defining a file appender
######################################################################
Log::Log4perl->init("$EG_DIR/log4j-manual-3.conf");

my $logger = Log::Log4perl->get_logger("");
$logger->debug("Gurgel");

ok(Log::Log4perl::Appender::TestBuffer->by_name("stdout")->buffer(), 
   'm#^\S+\s+\[N/A\] \(\S+?:\d+\) - Gurgel$#'); 
ok(Log::Log4perl::Appender::TestBuffer->by_name("R")->buffer(), 
   'm#^\S+\s+N/A\s+\'\' - Gurgel$#'); 

######################################################################
# Test the root logger via inheritance (discovered by Kevin Goess)
######################################################################
Log::Log4perl->reset();
Log::Log4perl::Appender::TestBuffer->reset();

Log::Log4perl->init("$EG_DIR/log4j-manual-3.conf");

$logger = Log::Log4perl->get_logger("foo");
$logger->debug("Gurgel");

ok(Log::Log4perl::Appender::TestBuffer->by_name("stdout")->buffer(), 
   'm#^\S+\s+\[N/A\] \(\S+?:\d+\) - Gurgel$#'); 
ok(Log::Log4perl::Appender::TestBuffer->by_name("R")->buffer(), 
    'm#^\S+\s+N/A \'foo\' - Gurgel$#'); 
