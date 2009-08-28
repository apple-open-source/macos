###########################################
# Test Suite for Log::Log4perl::Config
# Mike Schilli, 2002 (m@perlmeister.com)
###########################################

#########################
# change 'tests => 1' to 'tests => last_test_to_print';
#########################
use Test::More;

our $LOG_DISPATCH_PRESENT = 0;

BEGIN { 
    eval { require Log::Dispatch; };
    if($@) {
       plan skip_all => "only with Log::Dispatch";
    } else {
       $LOG_DISPATCH_PRESENT = 1;
       plan tests => 2;
    }
};

use Log::Log4perl;
use Log::Log4perl::Appender::TestBuffer;
use File::Spec;

my $EG_DIR = "eg";
$EG_DIR = "../eg" unless -d $EG_DIR;

ok(1); # If we made it this far, we're ok.

my $LOGFILE = "example.log";
unlink $LOGFILE; 

#Log::Log4perl->init(
#	File::Spec->catfile($EG_DIR, 'log4j-file-append-java.conf'));
Log::Log4perl->init("$EG_DIR/log4j-file-append-java.conf");


my $logger = Log::Log4perl->get_logger("");
$logger->debug("Gurgel");
$logger->info("Gurgel");
$logger->warn("Gurgel");
$logger->error("Gurgel");
$logger->fatal("Gurgel");

open FILE, "<$LOGFILE" or die "Cannot open $LOGFILE";
my $data = join '', <FILE>;
close FILE;

my $file = "t/006Config-Java.t";

my $exp = <<EOT;
$file 41 DEBUG N/A  - Gurgel
$file 42 INFO N/A  - Gurgel
$file 43 WARN N/A  - Gurgel
$file 44 ERROR N/A  - Gurgel
$file 45 FATAL N/A  - Gurgel
EOT

    # Adapt Win32 paths
$data =~ s#\\#/#g;

unlink $LOGFILE;
is($data, "$exp");
