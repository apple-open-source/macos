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
my $lines = ();
my $line = __LINE__ + 1;
push @lines, $line++; $logger->debug("Gurgel");
push @lines, $line++; $logger->info("Gurgel");
push @lines, $line++; $logger->warn("Gurgel");
push @lines, $line++; $logger->error("Gurgel");
push @lines, $line++; $logger->fatal("Gurgel");

open FILE, "<$LOGFILE" or die "Cannot open $LOGFILE";
my $data = join '', <FILE>;
close FILE;

my $file = "t/006Config-Java.t";

my $exp = <<EOT;
$file $lines[0] DEBUG N/A  - Gurgel
$file $lines[1] INFO N/A  - Gurgel
$file $lines[2] WARN N/A  - Gurgel
$file $lines[3] ERROR N/A  - Gurgel
$file $lines[4] FATAL N/A  - Gurgel
EOT

    # Adapt Win32 paths
$data =~ s#\\#/#g;

unlink $LOGFILE;
is($data, "$exp");
