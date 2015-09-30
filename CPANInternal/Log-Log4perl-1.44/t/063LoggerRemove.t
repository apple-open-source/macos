# http://stackoverflow.com/questions/5914088 and
# https://github.com/mschilli/log4perl/issues/7

use strict;
use Test::More;
use Log::Log4perl::Appender::TestBuffer;

plan tests => 6;

use Log::Log4perl qw(get_logger :easy);

# $Log::Log4perl::CHATTY_DESTROY_METHODS = 1;

my $conf = q(
log4perl.category.main             = WARN, LogBuffer
log4perl.category.Bar.Twix         = WARN, LogBuffer
log4perl.appender.LogBuffer  = Log::Log4perl::Appender::TestBuffer
log4perl.appender.LogBuffer.layout = \
Log::Log4perl::Layout::PatternLayout
log4perl.appender.LogBuffer.layout.ConversionPattern = %d %F{1} %L> %m %n
);

Log::Log4perl::init(\$conf);

my $buffer = Log::Log4perl::Appender::TestBuffer->by_name("LogBuffer");

my $logger = get_logger("Bar::Twix");

ok(exists $Log::Log4perl::Logger::LOGGERS_BY_NAME->{"Bar.Twix"}, 
   "logger exists");

Log::Log4perl->remove_logger( $logger );
undef $logger;

ok(!exists $Log::Log4perl::Logger::LOGGERS_BY_NAME->{"Bar.Twix"}, 
   "logger gone");

# now remove a stealth logger
$logger = get_logger("main");

ok(exists $Log::Log4perl::Logger::LOGGERS_BY_NAME->{"main"}, 
   "logger exists");

WARN "before";

Log::Log4perl->remove_logger( $logger );
undef $logger;

ok(!exists $Log::Log4perl::Logger::LOGGERS_BY_NAME->{"main"}, 
   "logger gone");

  # this should be a no-op now.
WARN "after";

like($buffer->buffer, qr/before/, "log message before logger removal present");
unlike($buffer->buffer, qr/after/, "log message after logger removal absent");
