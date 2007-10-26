#!/usr/bin/perl
# $Id: 024WarnDieCarp.t,v 1.1 2002/08/29 05:33:28 mschilli Exp $

# Check the various logFOO for FOO in {die, warn, Carp*}

# note: I <erik@selberg.com> prefer Test::Simple to just Test.

######################################################################
#
# This is a fairly simply smoketest... it basically runs the gamut of
# the warn / die / croak / cluck / confess / carp family and makes sure
# that the log output contained the appropriate string and STDERR 
# contains the appropriate string.
#
######################################################################

use warnings;
use strict;

use Test::Simple tests => 50;
use Log::Log4perl qw(get_logger);
use Log::Log4perl::Level;
use File::Spec;

my $warnstr;

# this nullifies warns and dies here... so testing the testscript may suck.
local $SIG{__WARN__} = sub { $warnstr = join("", @_); };
local $SIG{__DIE__} = sub { $warnstr = join("", @_); };

sub warndietest {
  my ($method, $in_str, $out_str, $app, $mname) = @_;

  eval { &$method($in_str) };
  
  ok($warnstr =~ /$out_str/, "$mname($in_str): STDERR contains \"$out_str\"");
  ok($app->buffer() =~ /$out_str/, "$mname($in_str): Buffer contains \"$out_str\"");
  $app->buffer("");
}

# same as above, just look for no output
sub warndietest_nooutput {
  my ($method, $in_str, $out_str, $app, $mname) = @_;

  eval { &$method($in_str) };
  
  ok($warnstr !~ /$out_str/, "$mname($in_str): STDERR does NOT contain \"$out_str\"");
  ok($app->buffer() !~ /$out_str/, "$mname($in_str): Buffer does NOT contain \"$out_str\"");
}

# same as above, just look for no output in buffer, but output in STDERR
sub dietest_nooutput {
  my ($method, $in_str, $out_str, $app, $mname) = @_;

  eval { &$method($in_str) };
  
  ok($warnstr =~ /$out_str/, "$mname($in_str): STDERR contains \"$out_str\"");
  ok($app->buffer() !~ /$out_str/, "$mname($in_str): Buffer does NOT contain \"$out_str\"");
}


ok(1, "Initialized OK"); 

############################################################
# Get a logger and use it without having called init() first
############################################################
my $log = Log::Log4perl::get_logger("abc.def");
my $app = Log::Log4perl::Appender->new(
    "Log::Log4perl::Appender::TestBuffer");
$log->add_appender($app);

######################################################################
# let's start testing!

$log->level($DEBUG);

my $test = 1;

######################################################################
# sanity: make sure the tests spit out FOO to the buffer and STDERR

foreach my $f ("logwarn", "logdie", "logcarp", "logcroak", "logcluck",
    "logconfess", "error_warn", "error_die") {
  warndietest(sub {$log->$f(@_)}, "Test $test: $f", "Test $test: $f", $app, "$f");
  $test++;
}

######################################################################
# change the log level to ERROR... warns should produce nothing now

$log->level($ERROR);

foreach my $f ("logdie", "logcroak", 
    "logconfess", "error_warn", "error_die") {
  warndietest(sub {$log->$f(@_)}, "Test $test: $f", "Test $test: $f", $app, "$f");
  $test++;
}

foreach my $f ("logwarn", "logcarp", "logcluck",
    ) {
  warndietest_nooutput(sub {$log->$f(@_)}, "Test $test: $f", "Test $test: $f", $app, "$f");
  $test++;
}

######################################################################
# change logging to OFF... FATALs still produce output though.

$log->level($OFF); # $OFF == $FATAL... although I suspect that's a bug in the log4j spec

foreach my $f ("logwarn", "logcarp", "logcluck", "error_warn") {
  warndietest_nooutput(sub {$log->$f(@_)}, "Test $test: $f", "Test $test: $f", $app, "$f");
  $test++;
}

foreach my $f ("error_die", "logdie", "logcroak", "logconfess") {
  dietest_nooutput(sub {$log->$f(@_)}, "Test $test: $f", "Test $test: $f", $app, "$f");
  $test++;
}

######################################################################
# Check if logdie %F%L lists the right file/line
######################################################################
Log::Log4perl->init(\<<'EOT');
    log4perl.rootLogger=DEBUG, A1
    log4perl.appender.A1=Log::Log4perl::Appender::TestBuffer
    log4perl.appender.A1.layout=org.apache.log4j.PatternLayout
    log4perl.appender.A1.layout.ConversionPattern=%F-%L: %m
EOT

my $logger = get_logger("Twix::Bar");

eval { $logger->logdie("Log and die!"); };

my $app0 = Log::Log4perl::Appender::TestBuffer->by_name("A1");
# print "Buffer: ", $app0->buffer(), "\n";

my $expected = File::Spec->catfile('t','024WarnDieCarp.t-132').": Log and die!";

ok($app0->buffer() eq $expected, "%F-%L adjustment, got ".$app0->buffer().", expected $expected");



