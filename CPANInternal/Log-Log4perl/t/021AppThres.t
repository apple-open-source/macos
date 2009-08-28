###########################################
# Test Suite for Log::Log4perl::Logger
# Mike Schilli, 2002 (m@perlmeister.com)
###########################################

use warnings;
use strict;

use Test::More;

use Log::Log4perl qw(get_logger);
use Log::Log4perl::Level;

BEGIN { plan tests => 20 }

ok(1); # If we made it this far, we're ok.

my $log0 =  Log::Log4perl->get_logger("");
my $log1 = Log::Log4perl->get_logger("abc.def");
my $log2 = Log::Log4perl->get_logger("abc.def.ghi");

$log0->level($DEBUG);
$log1->level($DEBUG);
$log2->level($DEBUG);

my $app0 = Log::Log4perl::Appender->new(
    "Log::Log4perl::Appender::TestBuffer");

my $app1 = Log::Log4perl::Appender->new(
    "Log::Log4perl::Appender::TestBuffer");

$app0->threshold($ERROR);  # As integer value
$app1->threshold("WARN");  # As string

$log0->add_appender($app0);
$log1->add_appender($app1);

##################################################
# Root logger's appender
##################################################
$app0->buffer("");
$app1->buffer("");
$log0->warn("Don't want to see this");
$log0->error("Yeah, log0");

is($app0->buffer(), "ERROR - Yeah, log0\n", "Threshold ERROR");
is($app1->buffer(), "", "Threshold WARN");

##################################################
# Inherited appender
##################################################
my $ret;

$app0->buffer("");
$app1->buffer("");

$ret = $log1->info("Don't want to see this");
is($ret, 0, "Info suppressed");

$ret = $log1->warn("Yeah, log1");
is($ret, 1, "inherited");

is($app0->buffer(), "", "inherited");
is($app1->buffer(), "WARN - Yeah, log1\n", "inherited");

##################################################
# Inherited appender over two hierarchies
##################################################
$app0->buffer("");
$app1->buffer("");
$log2->info("Don't want to see this");
$log2->error("Yeah, log2");

is($app0->buffer(), "ERROR - Yeah, log2\n", "two hierarchies");
is($app1->buffer(), "ERROR - Yeah, log2\n", "two hierarchies");

##################################################
# Appender threshold with config file
##################################################
# Reset appender population
Log::Log4perl::Appender::TestBuffer->reset();

my $conf = <<EOT;
log4perl.logger   = ERROR, BUF0
log4perl.logger.a = INFO, BUF1
log4perl.appender.BUF0           = Log::Log4perl::Appender::TestBuffer
log4perl.appender.BUF0.layout    = Log::Log4perl::Layout::SimpleLayout
log4perl.appender.BUF0.Threshold = ERROR
log4perl.appender.BUF1           = Log::Log4perl::Appender::TestBuffer
log4perl.appender.BUF1.layout    = Log::Log4perl::Layout::SimpleLayout
log4perl.appender.BUF1.Threshold = WARN
EOT

Log::Log4perl::init(\$conf);

$app0 = Log::Log4perl::Appender::TestBuffer->by_name("BUF0");
$app1 = Log::Log4perl::Appender::TestBuffer->by_name("BUF1");

my $loga = get_logger("a");

$loga->info("Don't want to see this");
$loga->error("Yeah, loga");

is($app0->buffer(), "ERROR - Yeah, loga\n", "appender threshold");
is($app1->buffer(), "ERROR - Yeah, loga\n", "appender threshold");

##################################################
# Appender threshold with config file and a Java
# Class
##################################################
# Reset appender population
Log::Log4perl::Appender::TestBuffer->reset();

$conf = <<EOT;
log4j.logger   = ERROR, BUF0
log4j.logger.a = INFO, BUF1
log4j.appender.BUF0           = org.apache.log4j.TestBuffer
log4j.appender.BUF0.layout    = SimpleLayout
log4j.appender.BUF0.Threshold = ERROR
log4j.appender.BUF1           = org.apache.log4j.TestBuffer
log4j.appender.BUF1.layout    = SimpleLayout
log4j.appender.BUF1.Threshold = WARN
EOT

Log::Log4perl::init(\$conf);

$app0 = Log::Log4perl::Appender::TestBuffer->by_name("BUF0");
$app1 = Log::Log4perl::Appender::TestBuffer->by_name("BUF1");

$loga = get_logger("a");

$loga->info("Don't want to see this");
$loga->error("Yeah, loga");

is($app0->buffer(), "ERROR - Yeah, loga\n", "threshold/java");
is($app1->buffer(), "ERROR - Yeah, loga\n", "threshold/java");

##################################################
# 'threshold' vs. 'Threshold'
##################################################
$conf = <<EOT;
log4j.logger   = ERROR, BUF0
log4j.logger.a = INFO, BUF1
log4j.appender.BUF0           = org.apache.log4j.TestBuffer
log4j.appender.BUF0.layout    = SimpleLayout
log4j.appender.BUF0.Threshold = ERROR
log4j.appender.BUF1           = org.apache.log4j.TestBuffer
log4j.appender.BUF1.layout    = SimpleLayout
log4j.appender.BUF1.threshold = WARN
EOT

eval { Log::Log4perl::init(\$conf); };

if($@) {
    like($@, qr/uppercase/, "warn on misspelled 'threshold'");
} else {
    ok(0, "Abort on misspelled 'threshold'");
}

##################################################
# Increase threshold of all appenders
##################################################
$conf = <<EOT;
log4perl.category                 = WARN, BUF0, BUF1

log4perl.appender.BUF0            = Log::Log4perl::Appender::TestBuffer
log4perl.appender.BUF0.Threshold  = WARN
log4perl.appender.BUF0.layout     = SimpleLayout

log4perl.appender.BUF1            = Log::Log4perl::Appender::TestBuffer
log4perl.appender.BUF1.Threshold  = ERROR
log4perl.appender.BUF1.layout     = SimpleLayout
EOT

Log::Log4perl::init(\$conf);

$app0 = Log::Log4perl::Appender::TestBuffer->by_name("BUF0");
$app1 = Log::Log4perl::Appender::TestBuffer->by_name("BUF1");

my $logger = get_logger("");

$logger->info("Info");
$logger->warn("Warning");
$logger->error("Error");

is($app0->buffer(), "WARN - Warning\nERROR - Error\n", "appender threshold");
is($app1->buffer(), "ERROR - Error\n", "appender threshold");

Log::Log4perl->appender_thresholds_adjust(-1);

$app0->buffer("");
$app1->buffer("");

$logger->more_logging();
$logger->info("Info");
$logger->warn("Warning");
$logger->error("Error");

is($app0->buffer(), "INFO - Info\nWARN - Warning\nERROR - Error\n", 
                    "adjusted appender threshold");
is($app1->buffer(), "WARN - Warning\nERROR - Error\n", 
                    "appender threshold");

$app0->buffer("");
$app1->buffer("");

   # reset previous thresholds
Log::Log4perl->appender_thresholds_adjust(1);

$app0->buffer("");
$app1->buffer("");

   # rig just one threshold
Log::Log4perl->appender_thresholds_adjust(-1, ['BUF0']);

$logger->more_logging();
$logger->info("Info");
$logger->warn("Warning");
$logger->error("Error");

is($app0->buffer(), "INFO - Info\nWARN - Warning\nERROR - Error\n", 
                    "adjusted appender threshold");
is($app1->buffer(), "ERROR - Error\n", 
                    "appender threshold");

