###########################################
# Test Suite for Log::Log4perl
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

Log::Log4perl->init(
        File::Spec->catfile($EG_DIR, 'log4j-file-append-perl.conf'));


my $logger = Log::Log4perl->get_logger("");
$logger->debug("Gurgel");
$logger->info("Gurgel");
$logger->warn("Gurgel");
$logger->error("Gurgel");
$logger->fatal("Gurgel");

open FILE, "<$LOGFILE" or die "Cannot open $LOGFILE";
my $data = join '', <FILE>;
close FILE;

my $file = "007LogPrio.t";

my $exp = <<EOT;
$file 30 DEBUG N/A  - Gurgel
$file 31 INFO N/A  - Gurgel
$file 32 WARN N/A  - Gurgel
$file 33 ERROR N/A  - Gurgel
$file 34 FATAL N/A  - Gurgel
EOT

unlink $LOGFILE;
ok($data, "$exp");
