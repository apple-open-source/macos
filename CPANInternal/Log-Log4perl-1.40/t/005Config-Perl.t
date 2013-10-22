###########################################
# Test Suite for Log::Log4perl::Config
# Mike Schilli, 2002 (m@perlmeister.com)
###########################################

BEGIN { 
    if($ENV{INTERNAL_DEBUG}) {
        require Log::Log4perl::InternalDebug;
        Log::Log4perl::InternalDebug->enable();
    }
}

#########################
# change 'tests => 1' to 'tests => last_test_to_print';
#########################
use Test::More;
BEGIN { plan tests => 3 };

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
my $line = __LINE__ + 1;
$logger->debug("Gurgel");

open LOG, "<$LOGFILE" or die "Cannot open $LOGFILE";
my $data = <LOG>;

END { close LOG; unlink $LOGFILE; }

is($data, "005Config-Perl.t $line DEBUG N/A  - Gurgel\n");

###############################################
# Check reading a config file via a file handle
###############################################
Log::Log4perl->reset();
open FILE, File::Spec->catfile($EG_DIR, 'log4j-file-append-perl.conf') or
    die "cannot open log4j-file-append-perl.conf";
Log::Log4perl->init(\*FILE);

$logger = Log::Log4perl->get_logger("");
$line = __LINE__ + 1;
$logger->debug("Gurgel");

$data = <LOG>;

is($data, "005Config-Perl.t $line DEBUG N/A  - Gurgel\n");
