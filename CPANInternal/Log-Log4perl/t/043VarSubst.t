#!/usr/bin/perl
##########################################################################
# Check basic variable substitution.
# Mike Schilli, 2003 (m@perlmeister.com)
##########################################################################
use warnings;
use strict;

use Test::More;
BEGIN { plan tests => 8 }
use Log::Log4perl qw(get_logger);

########################################################
# Wrong variable name
########################################################
my $conf = q(
screen = Log::Log4perl::Appender::Screen
log4perl.category = WARN, ScreenApp
log4perl.appender.ScreenApp = ${screen1}
log4perl.appender.ScreenApp.layout = \
    Log::Log4perl::Layout::PatternLayout
log4perl.appender.ScreenApp.layout.ConversionPattern = %d %F{1} %L> %m %n
);

eval { Log::Log4perl::init(\$conf) };

like($@, qr/Undefined Variable 'screen1'/);

########################################################
# Replacing appender class name
########################################################
$conf = q(
screen = Log::Log4perl::Appender::TestBuffer
log4perl.category = WARN, BufferApp
log4perl.appender.BufferApp = ${screen}
log4perl.appender.BufferApp.layout = \
    Log::Log4perl::Layout::PatternLayout
log4perl.appender.BufferApp.layout.ConversionPattern = %d %F{1} %L> %m %n
);

Log::Log4perl::init(\$conf);
my $logger = get_logger("");
$logger->error("foobar");
my $buffer = Log::Log4perl::Appender::TestBuffer->by_name("BufferApp");
like($buffer->buffer, qr/foobar/);

########################################################
# Replacing appender class name
########################################################
$conf = q(
    layout_class   = Log::Log4perl::Layout::PatternLayout
    layout_pattern = %d %F{1} %L> %m %n
    
    log4perl.category.Bar.Twix = WARN, Logfile, Screen

    log4perl.appender.Logfile  = Log::Log4perl::Appender::TestBuffer
    log4perl.appender.Logfile.filename = test.log
    log4perl.appender.Logfile.layout = ${layout_class}
    log4perl.appender.Logfile.layout.ConversionPattern = ${layout_pattern}

    log4perl.appender.Screen  = Log::Log4perl::Appender::TestBuffer
    log4perl.appender.Screen.layout = ${layout_class}
    log4perl.appender.Screen.layout.ConversionPattern = ${layout_pattern}
);

Log::Log4perl::init(\$conf);
$logger = get_logger("Bar::Twix");
$logger->error("foobar");

$buffer = Log::Log4perl::Appender::TestBuffer->by_name("Logfile");
like($buffer->buffer, qr/foobar/);
$buffer = Log::Log4perl::Appender::TestBuffer->by_name("Screen");
like($buffer->buffer, qr/foobar/);

########################################################
# Multi-Line variable
########################################################
$conf = q(
    layout_class   = \
Log::Log4perl::\
Layout::PatternLayout
    layout_pattern = %d %F{1} \
%L> \
%m \
%n
    log4perl.category.Bar.Twix = WARN, Logfile, Screen

    log4perl.appender.Logfile  = Log::Log4perl::Appender::TestBuffer
    log4perl.appender.Logfile.filename = test.log
    log4perl.appender.Logfile.layout = ${layout_class}
    log4perl.appender.Logfile.layout.ConversionPattern = ${layout_pattern}

    log4perl.appender.Screen  = Log::Log4perl::Appender::TestBuffer
    log4perl.appender.Screen.layout = ${layout_class}
    log4perl.appender.Screen.layout.ConversionPattern = ${layout_pattern}
);

Log::Log4perl::init(\$conf);
$logger = get_logger("Bar::Twix");
$logger->error("foobar");

$buffer = Log::Log4perl::Appender::TestBuffer->by_name("Logfile");
like($buffer->buffer, qr/foobar/);
$buffer = Log::Log4perl::Appender::TestBuffer->by_name("Screen");
like($buffer->buffer, qr/foobar/);

########################################################
# Environment variable substitution
########################################################
$ENV{layout_class}   = "Log::Log4perl::Layout::PatternLayout";
$ENV{layout_pattern} = "%d %F{1} %L> %m %n";

$conf = q(
    log4perl.category.Bar.Twix = WARN, Logfile, Screen

    log4perl.appender.Logfile  = Log::Log4perl::Appender::TestBuffer
    log4perl.appender.Logfile.filename = test.log
    log4perl.appender.Logfile.layout = ${layout_class}
    log4perl.appender.Logfile.layout.ConversionPattern = ${layout_pattern}

    log4perl.appender.Screen  = Log::Log4perl::Appender::TestBuffer
    log4perl.appender.Screen.layout = ${layout_class}
    log4perl.appender.Screen.layout.ConversionPattern = ${layout_pattern}
);

Log::Log4perl::init(\$conf);
$logger = get_logger("Bar::Twix");
$logger->error("foobar");

$buffer = Log::Log4perl::Appender::TestBuffer->by_name("Logfile");
like($buffer->buffer, qr/foobar/);
$buffer = Log::Log4perl::Appender::TestBuffer->by_name("Screen");
like($buffer->buffer, qr/foobar/);
