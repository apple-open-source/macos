###########################################
# Tests for Log4perl::DateFormat
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

BEGIN { plan tests => 36 }

use Log::Log4perl qw(get_logger);
use Log::Log4perl::DateFormat;
use Log::Log4perl::Appender::TestBuffer;

$Log::Log4perl::DateFormat::GMTIME = 1;

my $GMTIME = 1030429942 - 7*3600;

###########################################
# Year
###########################################
my $formatter = Log::Log4perl::DateFormat->new("yyyy yy yyyy");
is($formatter->format($GMTIME), "2002 02 2002");

###########################################
# Month
###########################################
$formatter = Log::Log4perl::DateFormat->new("MM M MMMM yyyy");
is($formatter->format($GMTIME), "08 8 August 2002");

###########################################
# Month
###########################################
$formatter = Log::Log4perl::DateFormat->new("MMM yyyy");
is($formatter->format($GMTIME), "Aug 2002");

###########################################
# Day-of-Month
###########################################
$formatter = Log::Log4perl::DateFormat->new("d ddd dd dddd yyyy");
is($formatter->format($GMTIME), "26 026 26 0026 2002");

###########################################
# am/pm Hour
###########################################
$formatter = Log::Log4perl::DateFormat->new("h hh hhh hhhh");
is($formatter->format($GMTIME), "11 11 011 0011");

###########################################
# 24 Hour
###########################################
$formatter = Log::Log4perl::DateFormat->new("H HH HHH HHHH");
is($formatter->format($GMTIME), "23 23 023 0023");

###########################################
# Minute
###########################################
$formatter = Log::Log4perl::DateFormat->new("m mm mmm mmmm");
is($formatter->format($GMTIME), "32 32 032 0032");

###########################################
# Second
###########################################
$formatter = Log::Log4perl::DateFormat->new("s ss sss ssss");
is($formatter->format($GMTIME), "22 22 022 0022");

###########################################
# Day of Week
###########################################
$formatter = Log::Log4perl::DateFormat->new("E EE EEE EEEE");
is($formatter->format($GMTIME), "Mon Mon Mon Monday");
is($formatter->format($GMTIME+24*60*60*1), "Tue Tue Tue Tuesday");
is($formatter->format($GMTIME+24*60*60*2), "Wed Wed Wed Wednesday");
is($formatter->format($GMTIME+24*60*60*3), "Thu Thu Thu Thursday");
is($formatter->format($GMTIME+24*60*60*4), "Fri Fri Fri Friday");
is($formatter->format($GMTIME+24*60*60*5), "Sat Sat Sat Saturday");
is($formatter->format($GMTIME+24*60*60*6), "Sun Sun Sun Sunday");

###########################################
# Day of Year
###########################################
$formatter = Log::Log4perl::DateFormat->new("D DD DDD DDDD");
is($formatter->format($GMTIME), "238 238 238 0238");

###########################################
# AM/PM
###########################################
$formatter = Log::Log4perl::DateFormat->new("a aa");
is($formatter->format($GMTIME), "PM PM");

###########################################
# Milliseconds
###########################################
$formatter = Log::Log4perl::DateFormat->new("S SS SSS SSSS SSSSS SSSSSS");
is($formatter->format($GMTIME, 123456), "1 12 123 1234 12345 123456");

###########################################
# Predefined formats
###########################################
$formatter = Log::Log4perl::DateFormat->new("DATE");
is($formatter->format($GMTIME, 123456), "26 Aug 2002 23:32:22,123");

$formatter = Log::Log4perl::DateFormat->new("ISO8601");
is($formatter->format($GMTIME, 123456), "2002-08-26 23:32:22,123");

$formatter = Log::Log4perl::DateFormat->new("ABSOLUTE");
is($formatter->format($GMTIME, 123456), "23:32:22,123");

$formatter = Log::Log4perl::DateFormat->new("APACHE");
is($formatter->format($GMTIME, 123456), "[Mon Aug 26 23:32:22 2002]");

###########################################
# Unknown
###########################################
$formatter = Log::Log4perl::DateFormat->new("xx K");
is($formatter->format($GMTIME), "xx -- 'K' not (yet) implemented --");

###########################################
# DDD bugfix
###########################################
$formatter = Log::Log4perl::DateFormat->new("DDD");
   # 1/1/2006
is($formatter->format(1136106000), "001");
$formatter = Log::Log4perl::DateFormat->new("D");
   # 1/1/2006
is($formatter->format(1136106000), "1");

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

like(Log::Log4perl::Appender::TestBuffer->by_name("Buffer")->buffer(), 
     qr/\d\d:\d\d:\d\d ERROR Blah/);

###########################################
# Allowing literal text in L4p >= 1.19
###########################################
my @tests = ( 
    q!yyyy-MM-dd'T'HH:mm:ss.SSS'Z'! => q!%04d-%02d-%02dT%02d:%02d:%02d.%sZ!,
    q!yyyy-MM-dd''HH:mm:ss.SSS''!   => q!%04d-%02d-%02d%02d:%02d:%02d.%s!,
    q!yyyy-MM-dd''''HH:mm:ss.SSS!   => q!%04d-%02d-%02d'%02d:%02d:%02d.%s!,
    q!yyyy-MM-dd''''''HH:mm:ss.SSS! => q!%04d-%02d-%02d''%02d:%02d:%02d.%s!,
    q!yyyy-MM-dd,HH:mm:ss.SSS!      => q!%04d-%02d-%02d,%02d:%02d:%02d.%s!,
    q!HH:mm:ss,SSS!                 => q!%02d:%02d:%02d,%s!,
    q!dd MMM yyyy HH:mm:ss,SSS!     => q!%02d %.3s %04d %02d:%02d:%02d,%s!,
    q!hh 'o''clock' a!              => q!%02d o'clock %1s!,
    q!hh 'o'clock' a!               => q!(undef)!,
    q!yyyy-MM-dd 'at' HH:mm:ss!     => q!%04d-%02d-%02d at %02d:%02d:%02d!,
);

#' calm down up vim syntax highlighting

while ( my ( $src, $expected ) = splice @tests, 0, 2 ) {
    my $df = eval { Log::Log4perl::DateFormat->new( $src ) };
    my $err = '';
    if ( $@ )
    {
        chomp $@;
        $err = "(error: $@)";
    }
    my $got = $df->{fmt} || '(undef)';
    is($got, $expected, "literal $src");
}
