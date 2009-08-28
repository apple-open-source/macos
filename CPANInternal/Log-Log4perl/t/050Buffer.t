###########################################
# Test Suite for 'Buffer' appender
# Mike Schilli, 2004 (m@perlmeister.com)
###########################################

use warnings;
use strict;

use Test::More tests => 6;
use Log::Log4perl::Appender::TestBuffer;

use Log::Log4perl qw(:easy);

my $conf = q(
log4perl.category                  = DEBUG, Buffer
log4perl.category.triggertest      = DEBUG, Buffer2

    # Regular Screen Appender
log4perl.appender.Screen           = Log::Log4perl::Appender::TestBuffer
log4perl.appender.Screen.layout    = PatternLayout
log4perl.appender.Screen.layout.ConversionPattern = %d %p %c %m %n

    # Buffering appender, using the appender above as outlet
log4perl.appender.Buffer               = Log::Log4perl::Appender::Buffer
log4perl.appender.Buffer.appender      = Screen
log4perl.appender.Buffer.trigger_level = ERROR

    # Second Screen Appender
log4perl.appender.Screen2           = Log::Log4perl::Appender::TestBuffer
log4perl.appender.Screen2.layout    = PatternLayout
log4perl.appender.Screen2.layout.ConversionPattern = %d %p %c %m %n

    # Buffering appender, with a subroutine reference as a trigger
log4perl.appender.Buffer2               = Log::Log4perl::Appender::Buffer
log4perl.appender.Buffer2.appender      = Screen2
log4perl.appender.Buffer2.trigger = sub {        \
         my($self, $params) = @_;               \
         return Log::Log4perl::Level::to_priority($params->{log4p_level}) >=       \
                Log::Log4perl::Level::to_priority('ERROR')  }

);

Log::Log4perl->init(\$conf);

my $buf = Log::Log4perl::Appender::TestBuffer->by_name("Screen");

DEBUG("This message gets buffered.");
is($buf->buffer(), "", "Buffering DEBUG");

INFO("This message gets buffered also.");
is($buf->buffer(), "", "Buffering INFO");

ERROR("This message triggers a buffer flush.");
like($buf->buffer(), qr/DEBUG.*?INFO.*?ERROR/s, "Flushing ERROR");


# testing trigger sub

my $buf2 = Log::Log4perl::Appender::TestBuffer->by_name("Screen2");

my $logger = Log::Log4perl->get_logger('triggertest');
$logger->debug("This message gets buffered.");
is($buf2->buffer(), "", "Buffering DEBUG");

$logger->info("This message gets buffered also.");
is($buf2->buffer(), "", "Buffering INFO");

$logger->error("This message triggers a buffer flush.");
like($buf2->buffer(), qr/DEBUG.*?INFO.*?ERROR/s, "Flushing ERROR");
