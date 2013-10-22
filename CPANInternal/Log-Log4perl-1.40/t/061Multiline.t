
# https://rt.cpan.org/Public/Bug/Display.html?id=60197

use Log::Log4perl;
use Log::Log4perl::Appender;
use Log::Log4perl::Appender::File;
use Log::Log4perl::Layout::PatternLayout::Multiline;

use Test::More tests => 1;

my $logger = Log::Log4perl->get_logger("blah");

my $layout = Log::Log4perl::Layout::PatternLayout::Multiline->new;

my $logfile = "./file.log";

my $appender = Log::Log4perl::Appender->new(
               "Log::Log4perl::Appender::File",
                    name => 'foo',
                    filename  => './file.log',
                    mode      => 'append',
                    autoflush => 1,
               );

# Set the appender's layout
$appender->layout($layout);
$logger->add_appender($appender);

# this message will be split into [], leading to undef being logged,
# which will cause most appenders (e.g. ::File) to warn
$appender->log({ level => 1, message => "\n\n" }, 'foo_category', 'INFO');

ok(1, "no warnings should appear here");

unlink $logfile;
