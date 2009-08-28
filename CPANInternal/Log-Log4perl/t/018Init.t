#Testing double-init

use Test;

use warnings;
use strict;

use Log::Log4perl;
use File::Spec;

my $WORK_DIR = "tmp";
if(-d "t") {
    $WORK_DIR = File::Spec->catfile(qw(t tmp));
}
unless (-e "$WORK_DIR"){
    mkdir("$WORK_DIR", 0755) || die "can't create $WORK_DIR ($!)";
}

my $testfilea = File::Spec->catfile(qw(t tmp test18a.log));
unlink $testfilea if (-e $testfilea);

my $testfileb = File::Spec->catfile(qw(t tmp test18b.log));
unlink $testfileb if (-e $testfileb);

BEGIN {plan tests => 2}
END { unlink $testfilea;
      unlink $testfileb;
    }

####################################################
# Double-Init, 2nd time with different log file name
####################################################
my $data = <<EOT;
log4j.category = INFO, FileAppndr
log4j.appender.FileAppndr          = Log::Log4perl::Appender::File
log4j.appender.FileAppndr.filename = $testfilea
log4j.appender.FileAppndr.layout   = Log::Log4perl::Layout::SimpleLayout
EOT

Log::Log4perl::init(\$data);
my $log = Log::Log4perl::get_logger("");

$log->info("Shu-wa-chi!");

$data = <<EOT;
log4j.category = INFO, FileAppndr
log4j.appender.FileAppndr          = Log::Log4perl::Appender::File
log4j.appender.FileAppndr.filename = $testfileb
log4j.appender.FileAppndr.layout   = Log::Log4perl::Layout::SimpleLayout
EOT

Log::Log4perl::init(\$data);
$log = Log::Log4perl::get_logger();

$log->info("Shu-wa-chi!");

# Check if both files contain one message each
for my $file ($testfilea, $testfileb) {
    open FILE, "<$file" or die "Cannot open $file";
    my $content = join '', <FILE>;
    close FILE;
    ok($content, "INFO - Shu-wa-chi!\n");
}
