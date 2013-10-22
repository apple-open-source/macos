###########################################
# Test Suite for Log::Log4perl
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
BEGIN { plan tests => 4 };

use Log::Log4perl qw(:easy);

#########################################################
# double newline
#########################################################
my $conf = q(
  log4perl.category = DEBUG, Buffer
  log4perl.appender.Buffer = Log::Log4perl::Appender::TestBuffer
  log4perl.appender.Buffer.layout = Log::Log4perl::Layout::PatternLayout
  log4perl.appender.Buffer.layout.ConversionPattern = %d %F{1} %L> %m%n
);

Log::Log4perl->init( \$conf );
my $buf = Log::Log4perl::Appender::TestBuffer->by_name("Buffer");

DEBUG "blah\n";
DEBUG "blah\n";

unlike($buf->buffer(), qr/blah\n\n/);

#########################################################
# turn default %m%n chomping feature off
#########################################################
$conf = q(
  log4perl.category = DEBUG, Buffer
  log4perl.appender.Buffer = Log::Log4perl::Appender::TestBuffer
  log4perl.appender.Buffer.layout = Log::Log4perl::Layout::PatternLayout
  log4perl.appender.Buffer.layout.ConversionPattern = %d %F{1} %L> %m%n
  log4perl.appender.Buffer.layout.message_chomp_before_newline = 0
);

Log::Log4perl->init( \$conf );
$buf = Log::Log4perl::Appender::TestBuffer->by_name("Buffer");

DEBUG "blah\n";
DEBUG "blah\n";
like($buf->buffer(), qr/blah\n\n/);

#########################################################
# %m without chomp
#########################################################
$conf = q(
  log4perl.category = DEBUG, Buffer
  log4perl.appender.Buffer = Log::Log4perl::Appender::TestBuffer
  log4perl.appender.Buffer.layout = Log::Log4perl::Layout::PatternLayout
  log4perl.appender.Buffer.layout.ConversionPattern = %m foo %n
);

Log::Log4perl->init( \$conf );
$buf = Log::Log4perl::Appender::TestBuffer->by_name("Buffer");

DEBUG "blah\n";
like($buf->buffer(), qr/blah\n foo/);

#########################################################
# try %m{chomp}
#########################################################
$conf = q(
  log4perl.category = DEBUG, Buffer
  log4perl.appender.Buffer = Log::Log4perl::Appender::TestBuffer
  log4perl.appender.Buffer.layout = Log::Log4perl::Layout::PatternLayout
  log4perl.appender.Buffer.layout.ConversionPattern = %m{chomp} foo %n
);

Log::Log4perl->init( \$conf );
$buf = Log::Log4perl::Appender::TestBuffer->by_name("Buffer");

DEBUG "blah\n";
DEBUG "blah\n";
like($buf->buffer(), qr/blah foo /);
