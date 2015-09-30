#!/usr/bin/perl
###########################################
# Syslog test cases
# Kevin Goess, cpan@goess.org 2002
###########################################
use warnings;
use strict;

use Log::Log4perl;
use Test;

our $RESULT_BUFFER;

package Log::MyOwnAppender;

our $IS_LOADED = 1;

use base qw(Log::Dispatch::Output);

sub new {
    my($proto, %params) = @_;
    my $class = ref $proto || $proto;

    my $self = bless {}, $class;

    $self->_basic_init(%params);

    return $self;
}


sub log_message {
    my $self = shift;
    my %params = @_;

    #params is { name    => \$appender_name,
    #            level   => 0,
    #            message => \$message,

    $main::RESULT_BUFFER = $params{level};
}


package main;


my $config = <<EOL;
log4j.category.plant      = DEBUG,  tappndr,syslogappndr

log4j.appender.tappndr        = Log::MyOwnAppender
log4j.appender.tappndr.layout = org.apache.log4j.SimpleLayout

log4j.appender.syslogappndr        = Log::Dispatch::Syslog
log4j.appender.syslogappndr.layout = org.apache.log4j.SimpleLayout


EOL


Log::Log4perl::init(\$config);

my $logger = Log::Log4perl::get_logger('plant');

$logger->fatal('foo');
ok($RESULT_BUFFER, 7);
$RESULT_BUFFER = undef;

$logger->error('foo');
ok($RESULT_BUFFER, 4);
$RESULT_BUFFER = undef;

$logger->warn('foo');
ok($RESULT_BUFFER, 3);
$RESULT_BUFFER = undef;

$logger->info('foo');
ok($RESULT_BUFFER, 1);
$RESULT_BUFFER = undef;

$logger->debug('foo'); 
ok($RESULT_BUFFER, 0);
$RESULT_BUFFER = undef;



BEGIN { plan tests => 5, }
