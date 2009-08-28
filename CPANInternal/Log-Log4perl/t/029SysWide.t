###########################################
# Test Suite for Log::Log4perl::Logger
# Mike Schilli, 2002 (m@perlmeister.com)
###########################################

use warnings;
use strict;

use Test;

use Log::Log4perl qw(get_logger);
use Log::Log4perl::Level;
use Log::Log4perl::Appender::TestBuffer;

BEGIN { plan tests => 3 }

ok(1); # If we made it this far, we're ok.

##################################################
# System-wide threshold
##################################################
# Reset appender population
Log::Log4perl::Appender::TestBuffer->reset();

my $conf = <<EOT;
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

my $app0 = Log::Log4perl::Appender::TestBuffer->by_name("BUF0");
my $app1 = Log::Log4perl::Appender::TestBuffer->by_name("BUF1");

my $loga = get_logger("a");

$loga->info("Don't want to see this");
$loga->error("Yeah, loga");

ok($app0->buffer(), "ERROR - Yeah, loga\n");
ok($app1->buffer(), "ERROR - Yeah, loga\n");
