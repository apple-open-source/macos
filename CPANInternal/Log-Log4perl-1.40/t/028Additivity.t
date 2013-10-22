###########################################
# Test Suite for Appender additivity
# Mike Schilli, 2002 (m@perlmeister.com)
###########################################

BEGIN { 
    if($ENV{INTERNAL_DEBUG}) {
        require Log::Log4perl::InternalDebug;
        Log::Log4perl::InternalDebug->enable();
    }
}

#########################
# change 'tests => 1' to 'tests => last_test_to_print';
#########################
use Test;
BEGIN { plan tests => 7 };

use Log::Log4perl qw(get_logger);
use Log::Log4perl::Appender::TestBuffer;

my $EG_DIR = "eg";
$EG_DIR = "../eg" unless -d $EG_DIR;

ok(1); # If we made it this far, we're ok.

######################################################################
# Define the root logger and another logger, additivity on
######################################################################
Log::Log4perl->init(\<<'EOT');
    log4perl.logger = INFO, A1
    log4perl.logger.Twix.Bar = DEBUG, A2
    log4perl.appender.A1=Log::Log4perl::Appender::TestBuffer
    log4perl.appender.A1.layout=Log::Log4perl::Layout::SimpleLayout
    log4perl.appender.A2=Log::Log4perl::Appender::TestBuffer
    log4perl.appender.A2.layout=Log::Log4perl::Layout::SimpleLayout
EOT

my $logger = get_logger("Twix::Bar");
$logger->info("Percolate this!");

my $buf1 = Log::Log4perl::Appender::TestBuffer->by_name("A1")->buffer();
my $buf2 = Log::Log4perl::Appender::TestBuffer->by_name("A2")->buffer();

ok($buf1, "INFO - Percolate this!\n");
ok($buf2, "INFO - Percolate this!\n");

Log::Log4perl::Appender::TestBuffer->reset();

######################################################################
# Define the root logger and another logger, additivity off
######################################################################
Log::Log4perl->init(\<<'EOT');
    log4perl.logger = INFO, A1
    log4perl.logger.Twix.Bar = DEBUG, A2
    log4perl.appender.A1=Log::Log4perl::Appender::TestBuffer
    log4perl.appender.A1.layout=Log::Log4perl::Layout::SimpleLayout
    log4perl.appender.A2=Log::Log4perl::Appender::TestBuffer
    log4perl.appender.A2.layout=Log::Log4perl::Layout::SimpleLayout
    log4perl.additivity.Twix.Bar = false
EOT

$logger = get_logger("Twix::Bar");
$logger->info("Percolate this!");

$buf1 = Log::Log4perl::Appender::TestBuffer->by_name("A1")->buffer();
$buf2 = Log::Log4perl::Appender::TestBuffer->by_name("A2")->buffer();

ok($buf1, ""); # Not supposed to show up in the root logger
ok($buf2, "INFO - Percolate this!\n");

Log::Log4perl::Appender::TestBuffer->reset();

######################################################################
# Define the root logger and another logger, additivity on explicitely
######################################################################
Log::Log4perl->init(\<<'EOT');
    log4perl.logger = INFO, A1
    log4perl.logger.Twix.Bar = DEBUG, A2
    log4perl.appender.A1=Log::Log4perl::Appender::TestBuffer
    log4perl.appender.A1.layout=Log::Log4perl::Layout::SimpleLayout
    log4perl.appender.A2=Log::Log4perl::Appender::TestBuffer
    log4perl.appender.A2.layout=Log::Log4perl::Layout::SimpleLayout
    log4perl.additivity.Twix.Bar = true
EOT

$logger = get_logger("Twix::Bar");
$logger->info("Percolate this!");

$buf1 = Log::Log4perl::Appender::TestBuffer->by_name("A1")->buffer();
$buf2 = Log::Log4perl::Appender::TestBuffer->by_name("A2")->buffer();

ok($buf1, "INFO - Percolate this!\n");
ok($buf2, "INFO - Percolate this!\n");

Log::Log4perl::Appender::TestBuffer->reset();
