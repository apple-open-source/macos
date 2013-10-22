
# https://rt.cpan.org/Public/Bug/Display.html?id=68105

use Log::Log4perl;
use Log::Log4perl::Appender;
use Log::Log4perl::Appender::File;

use Test::More tests => 1;

my $logfile = "test.log";
END { unlink $logfile; }

Log::Log4perl->init({
   'log4perl.rootLogger'                             => 'ALL, FILE',
   'log4perl.appender.FILE'                          =>
       'Log::Log4perl::Appender::File',
   'log4perl.appender.FILE.filename'                 => sub { "$logfile" },
   'log4perl.appender.FILE.layout'                   => 'SimpleLayout',
});

Log::Log4perl->get_logger->debug('yee haw');

open FILE, "<$logfile" or die $!;
my $data = join '', <FILE>;
close FILE;

is( $data, "DEBUG - yee haw\n", "hash-init with subref" );
