###########################################
# Test Suite for Log::Log4perl::Level
# Mike Schilli, 2008 (m@perlmeister.com)
###########################################

BEGIN { 
    if($ENV{INTERNAL_DEBUG}) {
        require Log::Log4perl::InternalDebug;
        Log::Log4perl::InternalDebug->enable();
    }
}

###########################################
  # Subclass L4p
package Mylogger;
use strict;
use Log::Log4perl;
our @ISA = qw(Log::Log4perl);

###########################################
package main;
use strict;

use Test::More;

plan tests => 1;

my $logger = Mylogger->get_logger("Waah");
is($logger->{category}, "Waah", "subclass category rt #32942");
