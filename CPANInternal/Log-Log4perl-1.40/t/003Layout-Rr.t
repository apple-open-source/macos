#!/usr/bin/perl

BEGIN { 
    if($ENV{INTERNAL_DEBUG}) {
        require Log::Log4perl::InternalDebug;
        Log::Log4perl::InternalDebug->enable();
    }
}

use strict;
use warnings;

use Test::More tests => 2;
use File::Spec;

use Log::Log4perl;
use Log::Log4perl::Layout::PatternLayout;
use Log::Log4perl::Level;
use Log::Log4perl::Appender::TestBuffer;

my ($SECONDS, $MICRO_SECONDS) = ($^T, 100_000); # Script's startup time
my $DEBUG = 0;


# Pretend that the script was at sleep
sub fake_sleep ($) {
    my ($seconds) = @_;
    $SECONDS += $seconds;
    $MICRO_SECONDS = ($MICRO_SECONDS + 1_000) % 1_000_000;
}

sub fake_time {
    return ($SECONDS, $MICRO_SECONDS);
}



my $logger = create_logger();


# Start some logging
$logger->info("Start");

fake_sleep(1);
$logger->debug("Pause: 1 sec");

fake_sleep(2);
$logger->info("Pause: 2 secs");

fake_sleep(1);
$logger->debug("Pause: 1 sec");

$logger->warn("End");

#  Debug traces to be turned on when troubleshooting
if ($DEBUG) {
    # Get the contents of the buffers
    foreach my $appender (qw(A B)) {
        my $buffer = Log::Log4perl::Appender::TestBuffer->by_name($appender)->buffer();
        diag("========= $appender ==========");
        diag($buffer);
    }
}

# Get the elapsed times so far
my @a = get_all_elapsed_ms('A');
my @b = get_all_elapsed_ms('B');

is_deeply(
    \@a,
    [
        'A 0ms Start [0ms]',
        'A 1001ms Pause: 1 sec [1001ms]',
        'A 2001ms Pause: 2 secs [3002ms]',
        'A 1001ms Pause: 1 sec [4003ms]',
        'A 0ms End [4003ms]',
    ]
);

is_deeply(
    \@b,
    [
        'B 0ms Start [0ms]',
        'B 3002ms Pause: 2 secs [3002ms]',
        'B 1001ms End [4003ms]',
    ]
);


#
# Returns the elapsed times logged so far.
#
sub get_all_elapsed_ms {
    my ($categoty) = @_;

    return split /\n/,
        Log::Log4perl::Appender::TestBuffer->by_name($categoty)->buffer()
    ;
}


#
# Initialize the logging system with a twist. Here we inject our own time
# function into the appenders. This way we will be able to control time and
# ensure a deterministic behaviour that can always be reproduced which is ideal
# for unit tests.
#
# We need to create the appenders by hand in order to add a custom time
# function. The final outcome should be something similar to the following
# configuration:
#
#   log4perl.logger.test = ALL, A, B
#   
#   log4perl.appender.A = Log::Log4perl::Appender::TestBuffer
#   log4perl.appender.A.layout = Log::Log4perl::Layout::PatternLayout
#   log4perl.appender.A.layout.ConversionPattern = A %Rms %m [%rms]%n
#   log4perl.appender.A.Threshold = ALL
#   
#   log4perl.appender.B = Log::Log4perl::Appender::TestBuffer
#   log4perl.appender.B.layout = Log::Log4perl::Layout::PatternLayout
#   log4perl.appender.B.layout.ConversionPattern = B %Rms %m [%rms]%n
#   log4perl.appender.B.Threshold = INFO
#
sub create_logger {

    my $logger = Log::Log4perl->get_logger("test");
    $logger->level($ALL);

    my %appenders = (
        A => $ALL,
        B => $INFO,
    );

    # Inject the time function into the appenders
    while (my ($name, $threshold) = each %appenders) {
        my $appender = Log::Log4perl::Appender->new(
            "Log::Log4perl::Appender::TestBuffer",
            name => $name,
        );
        if ($name eq 'B') {
            $appender->threshold($INFO);
        }

        my $layout = Log::Log4perl::Layout::PatternLayout->new(
            {time_function => \&fake_time},
            "$name %Rms %m [%rms]%n"
        );
        $appender->layout($layout);
        $logger->add_appender($appender);
    }
    
    return $logger;
}

