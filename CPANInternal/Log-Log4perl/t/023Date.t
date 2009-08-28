###########################################
# Tests for Log4perl::DateFormat
# Mike Schilli, 2002 (m@perlmeister.com)
###########################################

use warnings;
use strict;

use Test;

BEGIN { plan tests => 26 }

use Log::Log4perl qw(get_logger);
use Log::Log4perl::DateFormat;
use Log::Log4perl::Appender::TestBuffer;

$Log::Log4perl::DateFormat::GMTIME = 1;

my $GMTIME = 1030429942 - 7*3600;

###########################################
# Year
###########################################
my $formatter = Log::Log4perl::DateFormat->new("yyyy yy yyyy");
ok($formatter->format($GMTIME), "2002 02 2002");

###########################################
# Month
###########################################
$formatter = Log::Log4perl::DateFormat->new("MM M MMMM yyyy");
ok($formatter->format($GMTIME), "08 8 August 2002");

###########################################
# Month
###########################################
$formatter = Log::Log4perl::DateFormat->new("MMM yyyy");
ok($formatter->format($GMTIME), "Aug 2002");

###########################################
# Day-of-Month
###########################################
$formatter = Log::Log4perl::DateFormat->new("d ddd dd dddd yyyy");
ok($formatter->format($GMTIME), "26 026 26 0026 2002");

###########################################
# am/pm Hour
###########################################
$formatter = Log::Log4perl::DateFormat->new("h hh hhh hhhh");
ok($formatter->format($GMTIME), "11 11 011 0011");

###########################################
# 24 Hour
###########################################
$formatter = Log::Log4perl::DateFormat->new("H HH HHH HHHH");
ok($formatter->format($GMTIME), "23 23 023 0023");

###########################################
# Minute
###########################################
$formatter = Log::Log4perl::DateFormat->new("m mm mmm mmmm");
ok($formatter->format($GMTIME), "32 32 032 0032");

###########################################
# Second
###########################################
$formatter = Log::Log4perl::DateFormat->new("s ss sss ssss");
ok($formatter->format($GMTIME), "22 22 022 0022");

###########################################
# Day of Week
###########################################
$formatter = Log::Log4perl::DateFormat->new("E EE EEE EEEE");
ok($formatter->format($GMTIME), "Mon Mon Mon Monday");
ok($formatter->format($GMTIME+24*60*60*1), "Tue Tue Tue Tuesday");
ok($formatter->format($GMTIME+24*60*60*2), "Wed Wed Wed Wednesday");
ok($formatter->format($GMTIME+24*60*60*3), "Thu Thu Thu Thursday");
ok($formatter->format($GMTIME+24*60*60*4), "Fri Fri Fri Friday");
ok($formatter->format($GMTIME+24*60*60*5), "Sat Sat Sat Saturday");
ok($formatter->format($GMTIME+24*60*60*6), "Sun Sun Sun Sunday");

###########################################
# Day of Year
###########################################
$formatter = Log::Log4perl::DateFormat->new("D DD DDD DDDD");
ok($formatter->format($GMTIME), "238 238 238 0238");

###########################################
# AM/PM
###########################################
$formatter = Log::Log4perl::DateFormat->new("a aa");
ok($formatter->format($GMTIME), "PM PM");

###########################################
# Milliseconds
###########################################
$formatter = Log::Log4perl::DateFormat->new("S SS SSS SSSS SSSSS SSSSSS");
ok($formatter->format($GMTIME, 123456), "1 12 123 1234 12345 123456");

###########################################
# Predefined formats
###########################################
$formatter = Log::Log4perl::DateFormat->new("DATE");
ok($formatter->format($GMTIME, 123456), "26 Aug 2002 23:32:22,123");

$formatter = Log::Log4perl::DateFormat->new("ISO8601");
ok($formatter->format($GMTIME, 123456), "2002-08-26 23:32:22,123");

$formatter = Log::Log4perl::DateFormat->new("ABSOLUTE");
ok($formatter->format($GMTIME, 123456), "23:32:22,123");

$formatter = Log::Log4perl::DateFormat->new("APACHE");
ok($formatter->format($GMTIME, 123456), "[Mon Aug 26 23:32:22 2002]");

###########################################
# Unknown
###########################################
$formatter = Log::Log4perl::DateFormat->new("xx K");
ok($formatter->format($GMTIME), "xx -- 'K' not (yet) implemented --");

###########################################
# DDD bugfix
###########################################
$formatter = Log::Log4perl::DateFormat->new("DDD");
   # 1/1/2006
ok($formatter->format(1136106000), "001");
$formatter = Log::Log4perl::DateFormat->new("D");
   # 1/1/2006
ok($formatter->format(1136106000), "1");

###########################################
# In conjunction with Log4perl
###########################################
my $conf = q(
log4perl.category.Bar.Twix      = WARN, Buffer
log4perl.appender.Buffer        = Log::Log4perl::Appender::TestBuffer
log4perl.appender.Buffer.layout = \
    Log::Log4perl::Layout::PatternLayout
log4perl.appender.Buffer.layout.ConversionPattern = %d{HH:mm:ss} %p %m %n
);

Log::Log4perl::init(\$conf);

my $logger = get_logger("Bar::Twix");
$logger->error("Blah");

ok(Log::Log4perl::Appender::TestBuffer->by_name("Buffer")->buffer(), 
     qr/\d\d:\d\d:\d\d ERROR Blah/);


