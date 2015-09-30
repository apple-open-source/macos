#!/usr/bin/perl
###################################################################
# Check if a custom appender with a destroy handler gets its 
# warning through
###################################################################

BEGIN { 
    if($ENV{INTERNAL_DEBUG}) {
        require Log::Log4perl::InternalDebug;
        Log::Log4perl::InternalDebug->enable();
    }
}

package SomeAppender;
our @ISA = qw(Log::Log4perl::Appender);
sub new {
    bless {}, shift;
}
sub log {}
sub DESTROY {
    warn "Horrible Warning!";
}

package main;
use warnings;
use strict;
use Test::More;
use Log::Log4perl qw(:easy);

my $warnings;

$SIG{__WARN__} = sub {
    $warnings .= $_[0];
};

my $conf = q(
log4perl.category       = DEBUG, SomeA
log4perl.appender.SomeA = SomeAppender
log4perl.appender.SomeA.layout = Log::Log4perl::Layout::SimpleLayout
);

Log::Log4perl->init(\$conf);

plan tests => 1;

my $logger = get_logger();
$logger->debug("foo");

Log::Log4perl::Logger->cleanup();

END {
    like $warnings, qr/Horrible Warning!/, "app destruction warning caught";
}
