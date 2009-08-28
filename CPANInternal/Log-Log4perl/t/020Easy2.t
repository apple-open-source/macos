###########################################
# 020Easy2.t - more Easy tests
# Mike Schilli, 2004 (m@perlmeister.com)
###########################################
use warnings;
use strict;
use Log::Log4perl::Appender::TestBuffer;

my $stderr = "";

$SIG{__WARN__} = sub {
    #print "warn: <$_[0]>\n";
    $stderr .= $_[0];
};

use Test::More tests => 3;

use Log::Log4perl qw(:easy);

Log::Log4perl->init(\ q{
log4perl.category.Bar.Twix         = WARN, Term
log4perl.appender.Term          = Log::Log4perl::Appender::Screen
log4perl.appender.Term.layout = Log::Log4perl::Layout::SimpleLayout
});

    # This case caused a warning L4p 0.47
INFO "Boo!";

is($stderr, "", "no warning");

# Test new level TRACE

Log::Log4perl->init(\ q{
log4perl.category   = TRACE, Buf
log4perl.appender.Buf        = Log::Log4perl::Appender::TestBuffer
log4perl.appender.Buf.layout = Log::Log4perl::Layout::SimpleLayout
});

my $appenders = Log::Log4perl->appenders();
my $bufapp    = Log::Log4perl::Appender::TestBuffer->by_name("Buf");

TRACE("foobar");
is($bufapp->buffer(), "TRACE - foobar\n", "TRACE check");

Log::Log4perl->init(\ q{
log4perl.category   = DEBUG, Buf
log4perl.appender.Buf        = Log::Log4perl::Appender::TestBuffer
log4perl.appender.Buf.layout = Log::Log4perl::Layout::SimpleLayout
});
$bufapp    = Log::Log4perl::Appender::TestBuffer->by_name("Buf");

my $log = Log::Log4perl::get_logger("");
$log->trace("We don't want to see this");
is($bufapp->buffer(), "", "Suppressed trace() check");

