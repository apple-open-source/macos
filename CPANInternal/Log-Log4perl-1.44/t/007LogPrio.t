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

my $LOGFILE = "example-perl2.log";
unlink $LOGFILE;

Log::Log4perl->init( \ <<EOT );
log4j.rootLogger=DEBUG, LOGFILE

log4j.appender.LOGFILE=Log::Log4perl::Appender::File
log4j.appender.LOGFILE.filename=$LOGFILE
log4j.appender.LOGFILE.mode=append

log4j.appender.LOGFILE.layout=org.apache.log4j.PatternLayout
log4j.appender.LOGFILE.layout.ConversionPattern=%F{1} %L %p %t %c - %m%n
EOT

my $logger = Log::Log4perl->get_logger("");
my @lines = ();
my $line = __LINE__ + 1;
push @lines, $line++; $logger->debug("Gurgel");
push @lines, $line++; $logger->info("Gurgel");
push @lines, $line++; $logger->warn("Gurgel");
push @lines, $line++; $logger->error("Gurgel");
push @lines, $line++; $logger->fatal("Gurgel");

open FILE, "<$LOGFILE" or die "Cannot open $LOGFILE";
my $data = join '', <FILE>;
close FILE;

my $file = "007LogPrio.t";

my $exp = <<EOT;
$file $lines[0] DEBUG N/A  - Gurgel
$file $lines[1] INFO N/A  - Gurgel
$file $lines[2] WARN N/A  - Gurgel
$file $lines[3] ERROR N/A  - Gurgel
$file $lines[4] FATAL N/A  - Gurgel
EOT

unlink $LOGFILE;
ok($data, "$exp");
