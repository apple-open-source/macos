###########################################
# Test Suite for Log::Log4perl::Logger
# Mike Schilli, 2002 (m@perlmeister.com)
###########################################

BEGIN { 
    if($ENV{INTERNAL_DEBUG}) {
        require Log::Log4perl::InternalDebug;
        Log::Log4perl::InternalDebug->enable();
    }
}

use warnings;
use strict;

use Test;

use Log::Log4perl qw(get_logger);
use Log::Log4perl::Level;
use Log::Log4perl::Appender::TestBuffer;

BEGIN { plan tests => 2 }

ok(1); # If we made it this far, we're ok.

# Have TestBuffer log the Log::Dispatch priority
$Log::Log4perl::Appender::TestBuffer::LOG_PRIORITY = 1;
Log::Log4perl::Appender::TestBuffer->reset();

my $conf = <<EOT;
log4perl.logger   = ALL, BUF0
log4perl.appender.BUF0           = Log::Log4perl::Appender::TestBuffer
log4perl.appender.BUF0.layout    = Log::Log4perl::Layout::SimpleLayout
EOT

Log::Log4perl::init(\$conf);

my $app0 = Log::Log4perl::Appender::TestBuffer->by_name("BUF0");

my $loga = get_logger("a");

$loga->debug("debug");
$loga->info("info");
$loga->warn("warn");
$loga->error("error");
$loga->fatal("fatal");

ok($app0->buffer(), 
   "[0]: DEBUG - debug\n" .
   "[1]: INFO - info\n" .
   "[3]: WARN - warn\n" .
   "[4]: ERROR - error\n" .
   "[7]: FATAL - fatal\n" .
   ""
  );
