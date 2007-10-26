###########################################
# Tests for Log4perl used by a wrapper class
# Mike Schilli, 2002 (m@perlmeister.com)
###########################################
use warnings;
use strict;

use Test;

BEGIN { plan tests => 1 }

##################################################
package Wrapper::Log4perl;

use Log::Log4perl;
use Log::Log4perl::Level;

our @ISA = qw(Log::Log4perl);

sub get_logger {
    # This is highly stupid (object duplication) and definitely not what we 
    # want anybody to do, but just to have a test case for a logger in a 
    # wrapper package
    return Wrapper::Log4perl::Logger->new(@_);
}

##################################################
package Wrapper::Log4perl::Logger;
sub new {
    my $real_logger = Log::Log4perl::get_logger(@_);
    bless { real_logger => $real_logger }, $_[0];
}
sub AUTOLOAD {
    no strict;
    my $self = shift;
    $AUTOLOAD =~ s/.*:://;
    $self->{real_logger}->$AUTOLOAD(@_);
}
sub DESTROY {}

##################################################
package main;

use Log::Log4perl;
$Log::Log4perl::caller_depth = 1;
use Log::Log4perl::Level;

my $log0 = Wrapper::Log4perl->get_logger("");
$log0->level($DEBUG);

my $app0 = Log::Log4perl::Appender->new(
    "Log::Log4perl::Appender::TestBuffer");
my $layout = Log::Log4perl::Layout::PatternLayout->new(
    "File: %F{1} Line number: %L package: %C");
$app0->layout($layout);
$log0->add_appender($app0);

##################################################
my $rootlogger = Wrapper::Log4perl->get_logger("");
$rootlogger->debug("Hello");

ok($app0->buffer(), "File: 022Wrap.t Line number: 60 package: main");
