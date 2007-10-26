###########################################
# Test Suite for Log::Log4perl::Config
# Mike Schilli, 2002 (m@perlmeister.com)
###########################################

#########################
# change 'tests => 1' to 'tests => last_test_to_print';
#########################
use Test;
BEGIN { plan tests => 2 };


use Log::Log4perl;
use Log::Log4perl::Appender::TestBuffer;
use File::Spec;

my $EG_DIR = "eg";
$EG_DIR = "../eg" unless -d $EG_DIR;

ok(1); # If we made it this far, we're ok.

my $LOGFILE = "example.log";
unlink $LOGFILE;

Log::Log4perl->init(File::Spec->catfile($EG_DIR, 'log4j-file-append-perl.conf'));

my $logger = Log::Log4perl->get_logger("");
$logger->debug("Gurgel");

open FILE, "<$LOGFILE" or die "Cannot open $LOGFILE";
my $data = <FILE>;
close FILE;

unlink $LOGFILE;
ok($data, File::Spec->catfile(qw(t 005Config-Perl.t)) . 
          " 28 DEBUG N/A  - Gurgel\n");
