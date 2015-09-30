###########################################
# Tests for Log4perl used by a wrapper class
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

use Test::More;
use File::Basename;

BEGIN { plan tests => 5 }

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
Log::Log4perl->wrapper_register(__PACKAGE__);
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
local $Log::Log4perl::caller_depth =
    $Log::Log4perl::caller_depth + 1;
use Log::Log4perl::Level;

my $log0 = Wrapper::Log4perl->get_logger("");
$log0->level($DEBUG);

my $app0 = Log::Log4perl::Appender->new(
    "Log::Log4perl::Appender::TestBuffer");
my $layout = Log::Log4perl::Layout::PatternLayout->new(
    "File: %F{1} Line number: %L package: %C trace: %T");
$app0->layout($layout);
$log0->add_appender($app0);

##################################################
my $rootlogger = Wrapper::Log4perl->get_logger("");
my $line = __LINE__ + 1;
$rootlogger->debug("Hello");

my $buf = $app0->buffer();
$buf =~ s#(\S+022Wrap\.t)#basename( $1 )#eg;

# [rt 74836] Carp.pm added a dot at the end with 1.25. 
# Be dot-agnostic.
$buf =~ s/\.$//;

is($buf,
    "File: 022Wrap.t Line number: $line package: main " .
    "trace: at 022Wrap.t line $line",
   "appender check");

  # with the new wrapper_register in Log4perl 1.29, this will even work
  # *without* modifying caller_depth
$Log::Log4perl::caller_depth--;
$app0->buffer("");
$line = __LINE__ + 1;
$rootlogger->debug("Hello");

  # Win32
# [rt 74836] Carp.pm added a dot at the end with 1.25. 
# Be dot-agnostic.
$buf = $app0->buffer();
$buf =~ s/\.$//;
$buf =~ s#(\S+022Wrap\.t)#basename( $1 )#eg;

is($buf,
    "File: 022Wrap.t Line number: $line package: main " .
    "trace: at 022Wrap.t line $line",
   "appender check");

##################################################
package L4p::Wrapper;
Log::Log4perl->wrapper_register(__PACKAGE__);
no strict qw(refs);
*get_logger = sub {

    my @args = @_;

    if(defined $args[0] and $args[0] eq __PACKAGE__) {
         $args[0] =~ s/__PACKAGE__/Log::Log4perl/g;
    }
    Log::Log4perl::get_logger( @args );
};

package main;

my $logger = L4p::Wrapper::get_logger();
is $logger->{category}, "main", "cat on () is main";

$logger = L4p::Wrapper::get_logger(__PACKAGE__);
is $logger->{category}, "main", "cat on (__PACKAGE__) is main";

$logger = L4p::Wrapper->get_logger();
is $logger->{category}, "main", "cat on ->() is main";

# use Data::Dumper;
# print Dumper($logger);
