###########################################
# Test Suite Log::Log4perl::NDC
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
use Log::Log4perl::NDC;
use Log::Log4perl::MDC;

BEGIN { plan tests => 3 }

# Have TestBuffer log the Log::Dispatch priority
Log::Log4perl::Appender::TestBuffer->reset();

my $conf = <<EOT;
log4perl.logger   = ALL, BUF0
log4perl.appender.BUF0           = Log::Log4perl::Appender::TestBuffer
log4perl.appender.BUF0.layout    = Log::Log4perl::Layout::PatternLayout
log4perl.appender.BUF0.layout.ConversionPattern = %m <%x>
EOT

Log::Log4perl::init(\$conf);

my $app0 = Log::Log4perl::Appender::TestBuffer->by_name("BUF0");

my $loga = get_logger("a");

Log::Log4perl::NDC->push("first");
$loga->debug("debug");

    # Push more than MAX
Log::Log4perl::NDC->push("second");
Log::Log4perl::NDC->push("third");
Log::Log4perl::NDC->push("fourth");
Log::Log4perl::NDC->push("fifth");
Log::Log4perl::NDC->push("sixth");
$loga->info("info");

    # Delete NDC stack
Log::Log4perl::NDC->remove();
$loga->warn("warn");

Log::Log4perl::NDC->push("seventh");
$loga->error("error");

ok($app0->buffer(), 
   "debug <first>info <first second third fourth sixth>warn <[undef]>error <seventh>");

Log::Log4perl::Appender::TestBuffer->reset();

Log::Log4perl::MDC->put("remote_host", "blah-host");
Log::Log4perl::MDC->put("ip", "blah-ip");

$conf = <<EOT;
log4perl.logger   = ALL, BUF1
log4perl.appender.BUF1           = Log::Log4perl::Appender::TestBuffer
log4perl.appender.BUF1.layout    = Log::Log4perl::Layout::PatternLayout
log4perl.appender.BUF1.layout.ConversionPattern = %X{remote_host}: %m %X{ip}%n
EOT

Log::Log4perl::init(\$conf);

my $app1 = Log::Log4perl::Appender::TestBuffer->by_name("BUF1");

my $logb = get_logger("b");

$logb->debug("testmessage");

ok($app1->buffer(), 
   "blah-host: testmessage blah-ip\n");

# Check what happens if %X is used with an undef value
Log::Log4perl::Appender::TestBuffer->reset();

$conf = <<EOT;
log4perl.logger   = ALL, BUF1
log4perl.appender.BUF1           = Log::Log4perl::Appender::TestBuffer
log4perl.appender.BUF1.layout    = Log::Log4perl::Layout::PatternLayout
log4perl.appender.BUF1.layout.ConversionPattern = %X{quack}: %m %X{ip}%n
EOT

Log::Log4perl::init(\$conf);

$app1 = Log::Log4perl::Appender::TestBuffer->by_name("BUF1");

$logb = get_logger("b");

$logb->debug("testmessage");

ok($app1->buffer(), 
   "[undef]: testmessage blah-ip\n");
