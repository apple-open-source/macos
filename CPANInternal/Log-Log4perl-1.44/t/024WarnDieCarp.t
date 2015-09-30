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

BEGIN { 
    if($ENV{INTERNAL_DEBUG}) {
        require Log::Log4perl::InternalDebug;
        Log::Log4perl::InternalDebug->enable();
    }
}

use warnings;
use strict;

use Test::More;
use Log::Log4perl qw(get_logger :easy);
use Log::Log4perl::Level;
use File::Spec; use Data::Dumper;

BEGIN {
    if ($] < 5.006) {
        plan skip_all => "Only with perl >= 5.006";
    } else {
        plan tests => 73;
    }
}

my $warnstr;

# this nullifies warns and dies here... so testing the testscript may suck.
local $SIG{__WARN__} = sub { $warnstr = join("", @_); };
local $SIG{__DIE__} = sub { $warnstr = join("", @_); };

sub warndietest {
  my ($method, $in_str, $out_str, $app, $mname) = @_;

  eval { &$method($in_str) };
  
  like($warnstr, qr/$out_str/, 
       "$mname($in_str): STDERR contains \"$out_str\"");
  like($app->buffer(), qr/$out_str/, 
       "$mname($in_str): Buffer contains \"$out_str\"");
  $app->buffer("");
}

# same as above, just look for no output
sub warndietest_nooutput {
  my ($method, $in_str, $out_str, $app, $mname) = @_;

  eval { &$method($in_str) };
  
  unlike($warnstr, qr/\Q$out_str\E/, 
       "$mname($in_str): STDERR does NOT contain \"$out_str\"");
  unlike($app->buffer(), qr/$out_str/, 
       "$mname($in_str): Buffer does NOT contain \"$out_str\"");
}

# warn() still prints to stderr, but nothing gets logged
sub warndietest_stderronly {
  my ($method, $in_str, $out_str, $app, $mname) = @_;

  eval { &$method($in_str) };
  
  my($pkg, $file, $line) = caller();

    # it's in stderr
  like($warnstr, qr/\Q$out_str\E/, 
       "$mname($in_str): STDERR does contain \"$out_str\" ($file:$line)");
    # but not logged by log4perl
  unlike($app->buffer(), qr/$out_str/, 
       "$mname($in_str): Buffer does NOT contain \"$out_str\" ($file:$line)");
}

# same as above, just look for no output in buffer, but output in STDERR
sub dietest_nooutput {
  my ($method, $in_str, $out_str, $app, $mname) = @_;

  eval { &$method($in_str) };
  
  like($warnstr, qr/$out_str/, "$mname($in_str): STDERR contains \"$out_str\"");
  unlike($app->buffer(), qr/$out_str/, 
         "$mname($in_str): Buffer does NOT contain \"$out_str\"");
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
# lets start testing!

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
# change the log level to ERROR... warns should produce nothing in 
# log4perl now, but logwarn still triggers warn()

$log->level($ERROR);

foreach my $f ("logdie", "logcroak", 
    "logconfess", "error_warn", "error_die") {
  warndietest(sub {$log->$f(@_)}, "Test $test: $f", "Test $test: $f", $app, "$f");
  $test++;
}

foreach my $f ("logwarn", "logcarp", "logcluck",
    ) {
  warndietest_stderronly(sub {$log->$f(@_)}, "Test $test: $f", "Test $test: $f", $app, "$f");
  $test++;
}

######################################################################
# change logging to OFF... FATALs still produce output though.

$log->level($OFF); # $OFF == $FATAL... although I suspect thats a bug in the log4j spec

foreach my $f ("logwarn", "logcarp", "logcluck", "error_warn") {
  warndietest_stderronly(sub {$log->$f(@_)}, "Test $test: $f", "Test $test: $f", $app, "$f");
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

my $line_number = __LINE__ + 1;
eval { $logger->logdie("Log and die!"); };

my $app0 = Log::Log4perl::Appender::TestBuffer->by_name("A1");
# print "Buffer: ", $app0->buffer(), "\n";

like($app0->buffer(), qr/024WarnDieCarp.t-$line_number: Log and die!/,
   "%F-%L adjustment");

######################################################################
# Check if logcarp/cluck/croak are reporting the calling package,
# not the one the error happened in.
######################################################################
$app0->buffer("");

package Weirdo;
our $foo_line;
our $bar_line;

use Log::Log4perl qw(get_logger);
sub foo {
    my $logger = get_logger("Twix::Bar");
    $foo_line = __LINE__ + 1;
    $logger->logcroak("Inferno!");
}
sub bar {
    my $logger = get_logger("Twix::Bar");
    $bar_line = __LINE__ + 1;
    $logger->logdie("Inferno!");
}

package main;
eval { Weirdo::foo(); };

like($app0->buffer(), qr/$Weirdo::foo_line/,
   "Check logcroak/Carp");

$app0->buffer("");
eval { Weirdo::bar(); };

like($app0->buffer(), qr/$Weirdo::bar_line/,
   "Check logdie");

######################################################################
# Check if logcarp/cluck/croak are reporting the calling package,
# when they are more than one hierarchy from the top.
######################################################################
$app0->buffer("");

package Foo;
our $foo_line;
use Log::Log4perl qw(get_logger);
sub foo {
    my $logger = get_logger("Twix::Bar");
    $foo_line = __LINE__ + 1;
    $logger->logcarp("Inferno!");
}

package Bar;
sub bar {
    Foo::foo();
}

package main;
eval { Bar::bar(); };

SKIP: {
    use Carp; 
    skip "Detected buggy Carp.pm (upgrade to perl-5.8.*)", 1 unless 
        defined $Carp::VERSION;
    like($app0->buffer(), qr/$Foo::foo_line/,
       "Check logcarp");
}

######################################################################
# Test fix of bug that had logwarn/die/etc print unformatted messages.
######################################################################
$logger = get_logger("Twix::Bar");
$log->level($DEBUG);

eval { $logger->logdie(sub { "a" . "-" . "b" }); };
like($@, qr/a-b/, "bugfix: logdie with sub{} as argument");

$logger->logwarn(sub { "a" . "-" . "b" });
like($warnstr, qr/a-b/, "bugfix: logwarn with sub{} as argument");

$logger->logwarn({ filter => \&Dumper,
                   value  => "a-b" });
like($warnstr, qr/a-b/, "bugfix: logwarn with sub{filter/value} as argument");

eval { $logger->logcroak({ filter => \&Dumper,
                    value  => "a-b" }); };
like($warnstr, qr/a-b/, "bugfix: logcroak with sub{} as argument");

######################################################################
# logcroak/cluck/carp/confess level test
######################################################################
our($carp_line, $call_line);

package Foo1;
use Log::Log4perl qw(:easy);
sub foo { get_logger("Twix::Bar")->logcarp("foocarp"); $carp_line = __LINE__ }

package Bar1;
sub bar { Foo1::foo(); $call_line = __LINE__; }

package main;

my $l4p_app = $Log::Log4perl::Logger::APPENDER_BY_NAME{"A1"};
my $layout = Log::Log4perl::Layout::PatternLayout->new("%M#%L %m%n");
$l4p_app->layout($layout);

$app0->buffer("");
Foo1::foo(); $call_line = __LINE__;
  # Foo1::foo#238 foocarp at 024WarnDieCarp.t line 250
like($app0->buffer(), qr/Foo1::foo#$carp_line foocarp.*$call_line/,
     "carp in subfunction");
    # foocarp at 024WarnDieCarp.t line 250
like($warnstr, qr/foocarp.*line $call_line/, "carp output");

$app0->buffer("");
Bar1::bar(); 

SKIP: {
    use Carp; 
    skip "Detected buggy Carp.pm (upgrade to perl-5.8.*)", 1 unless 
        defined $Carp::VERSION;

    # Foo1::foo#238 foocarp at 024WarnDieCarp.t line 250
    like($app0->buffer(), qr/Foo1::foo#$carp_line foocarp.*$call_line/,
       "carp in sub-sub-function");
}

    # foocarp at 024WarnDieCarp.t line 250
like($warnstr, qr/foocarp.*line $call_line/, "carp output");

######################################################################
# logconfess fix (1.12)
######################################################################
$app0->buffer("");

package Foo1;
sub new {
    my($class) = @_;
    bless {}, $class;
}

sub foo1 {
    my $log = get_logger();
    $log->logconfess("bah!");
}

package main;

my $foo = Foo1->new();
eval { $foo->foo1() };

like $@, qr/024WarnDieCarp.*Foo1::foo1.*eval/s, "Confess logs correct frame";

######################################################################
# logdie/warn caller level bug
######################################################################
Log::Log4perl->init(\<<'EOT');
    log4perl.rootLogger=DEBUG, A1
    log4perl.appender.A1=Log::Log4perl::Appender::TestBuffer
    log4perl.appender.A1.layout=org.apache.log4j.PatternLayout
    log4perl.appender.A1.layout.ConversionPattern=%F-%L: %m
EOT

$logger = get_logger("Twix::Bar");

$logger->logwarn("warn!");
like $warnstr, qr/024WarnDieCarp/, "logwarn() caller depth bug";
unlike $warnstr, qr/Logger.pm/, "logwarn() caller depth bug";

$Log::Log4perl::Logger::DIE_DEBUG = 1;
$logger->logdie("die!");
like $Log::Log4perl::Logger::DIE_DEBUG_BUFFER, qr/024WarnDieCarp/, 
     "logdie() caller depth bug";
unlike $Log::Log4perl::Logger::DIE_DEBUG_BUFFER, qr/Logger.pm/, 
     "logdie() caller depth bug";

my $app3 = Log::Log4perl::Appender::TestBuffer->by_name("A1");
$app3->buffer("");

my $line1 = __LINE__ + 1;
subroutine();

my $line2;
sub subroutine {
    $line2 = __LINE__ + 1;
    $logger->logcluck("cluck!");
}

like $app3->buffer(), qr/-$line2: cluck!/, "logcluck()";
like $app3->buffer(), qr/main::subroutine\(\) called .* line $line1/, 
     "logcluck()";

# Carp test

$app3->buffer("");
my $line3 = __LINE__ + 1;
subroutine_carp();

my $line4;
sub subroutine_carp {
    $line4 = __LINE__ + 1;
    $logger->logcarp("carp!");
}

like $app3->buffer(), qr/-$line4: carp!/, "logcarp()";
like $app3->buffer(), qr/main::subroutine_carp\(\) called .* line $line3/, 
     "logcarp()";

# Stringify test
$Log::Log4perl::Logger::DIE_DEBUG = 0;
$Log::Log4perl::STRINGIFY_DIE_MESSAGE = 0;

eval {
    $logger->logcroak( { foo => "bar" } );
};

is $@->{ foo }, "bar", "croak without stringify";

eval {
    $logger->logconfess( { foo => "bar" } );
};

is $@->{ foo }, "bar", "confess without stringify";

eval {
    $logger->logdie( { foo => "bar" } );
};

is $@->{ foo }, "bar", "die without stringify";
