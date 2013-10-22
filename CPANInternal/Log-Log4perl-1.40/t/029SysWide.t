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

BEGIN { plan tests => 6 }

ok(1); # If we made it this far, we're ok.

##################################################
# System-wide threshold
##################################################
# Reset appender population
Log::Log4perl::Appender::TestBuffer->reset();

my $conf = <<EOT;
log4perl.logger.a = INFO, BUF0
log4perl.appender.BUF0           = Log::Log4perl::Appender::TestBuffer
log4perl.appender.BUF0.layout    = Log::Log4perl::Layout::SimpleLayout
log4perl.threshold = ERROR
EOT

Log::Log4perl::init(\$conf);

my $app0 = Log::Log4perl::Appender::TestBuffer->by_name("BUF0");

my $loga = get_logger("a");

$loga->info("Don't want to see this");
$loga->error("Yeah, loga");

ok($app0->buffer(), "ERROR - Yeah, loga\n");

##################################################
# System-wide threshold with appender threshold
##################################################
# Reset appender population
Log::Log4perl::Appender::TestBuffer->reset();

$conf = <<EOT;
log4perl.logger   = ERROR, BUF0
log4perl.logger.a = INFO, BUF1
log4perl.appender.BUF0           = Log::Log4perl::Appender::TestBuffer
log4perl.appender.BUF0.layout    = Log::Log4perl::Layout::SimpleLayout
log4perl.appender.BUF0.Threshold = WARN
log4perl.appender.BUF1           = Log::Log4perl::Appender::TestBuffer
log4perl.appender.BUF1.layout    = Log::Log4perl::Layout::SimpleLayout
log4perl.appender.BUF1.Threshold = INFO
log4perl.threshold = ERROR
EOT

Log::Log4perl::init(\$conf);

$app0 = Log::Log4perl::Appender::TestBuffer->by_name("BUF0");
my $app1 = Log::Log4perl::Appender::TestBuffer->by_name("BUF1");

$loga = get_logger("a");

$loga->info("Don't want to see this");
$loga->error("Yeah, loga");

ok($app0->buffer(), "ERROR - Yeah, loga\n");
ok($app1->buffer(), "ERROR - Yeah, loga\n");

############################################################
# System-wide threshold shouldn't lower appender thresholds
############################################################
# Reset appender population
Log::Log4perl::Appender::TestBuffer->reset();

$conf = q(
log4perl.threshold = DEBUG
log4perl.category = INFO, BUF0
log4perl.appender.BUF0.Threshold = WARN
log4perl.appender.BUF0           = Log::Log4perl::Appender::TestBuffer
log4perl.appender.BUF0.layout    = Log::Log4perl::Layout::SimpleLayout
);

Log::Log4perl::init(\$conf);

my $logger = get_logger();
$logger->info("Blah");

$app0 = Log::Log4perl::Appender::TestBuffer->by_name("BUF0");
ok($app0->buffer(), "", "syswide threshold shouldn't lower app thresholds");

############################################################
# System-wide threshold shouldn't lower appender thresholds
############################################################
# Reset appender population
Log::Log4perl::Appender::TestBuffer->reset();

$conf = q(
log4perl.threshold = ERROR
log4perl.category = INFO, BUF0
log4perl.appender.BUF0.Threshold = DEBUG
log4perl.appender.BUF0           = Log::Log4perl::Appender::TestBuffer
log4perl.appender.BUF0.layout    = Log::Log4perl::Layout::SimpleLayout
);

Log::Log4perl::init(\$conf);

$logger = get_logger();
$logger->warn("Blah");

$app0 = Log::Log4perl::Appender::TestBuffer->by_name("BUF0");
ok($app0->buffer(), "", "syswide threshold trumps thresholds");
