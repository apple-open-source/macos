###########################################
# Test Suite for Log::Log4perl::Resurrector
# Mike Schilli, 2007 (m@perlmeister.com)
###########################################

BEGIN { 
    if($ENV{INTERNAL_DEBUG}) {
        require Log::Log4perl::InternalDebug;
        Log::Log4perl::InternalDebug->enable();
    }
}

use strict;
use Test::More;
use Log::Log4perl qw(:easy);

BEGIN {
    my $eg = "eg";
    $eg = "../eg" unless -d $eg;
    push @INC, $eg;
};

use Log::Log4perl::Resurrector;
use L4pResurrectable;

plan tests => 1;

Log::Log4perl->init(\ <<'EOT');
  log4perl.logger = DEBUG, A1
  log4perl.appender.A1        = Log::Log4perl::Appender::TestBuffer
  log4perl.appender.A1.layout = Log::Log4perl::Layout::SimpleLayout
EOT

my $buffer = Log::Log4perl::Appender::TestBuffer->by_name("A1");

L4pResurrectable::foo();
is($buffer->buffer(), "DEBUG - foo was here\nINFO - bar was here\n", 
   "resurrected statement");
