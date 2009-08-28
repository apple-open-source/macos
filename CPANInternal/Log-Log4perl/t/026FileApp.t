#Testing if the file-appender appends in default mode

use Test;

use warnings;
use strict;

use Log::Log4perl;
use File::Spec;

our $LOG_DISPATCH_PRESENT = 0;

BEGIN { 
    eval { require Log::Dispatch; };
    if(! $@) {
       $LOG_DISPATCH_PRESENT = 1;
    }
};

my $WORK_DIR = "tmp";
if(-d "t") {
    $WORK_DIR = File::Spec->catfile(qw(t tmp));
}
unless (-e "$WORK_DIR"){
    mkdir("$WORK_DIR", 0755) || die "can't create $WORK_DIR ($!)";
}

my $testfile = File::Spec->catfile($WORK_DIR, "test26.log");

BEGIN {plan tests => 15}

END { unlink $testfile;
      unlink "${testfile}_1";
      unlink "${testfile}_2";
      unlink "${testfile}_3";
    }

####################################################
#  First, preset the log file with some content
####################################################
open FILE, ">$testfile" or die "Cannot create $testfile";
print FILE "previous content\n";
close FILE;

####################################################
# Append to a log file without specifying append mode
# explicitely
####################################################
my $data = <<EOT;
log4j.category = INFO, FileAppndr
log4j.appender.FileAppndr          = Log::Log4perl::Appender::File
log4j.appender.FileAppndr.filename = $testfile
log4j.appender.FileAppndr.layout   = Log::Log4perl::Layout::SimpleLayout
EOT

Log::Log4perl::init(\$data);
my $log = Log::Log4perl::get_logger("");
$log->info("Shu-wa-chi!");

open FILE, "<$testfile" or die "Cannot create $testfile";
my $content = join '', <FILE>;
close FILE;

ok($content, "previous content\nINFO - Shu-wa-chi!\n");

####################################################
# Clobber the log file if overwriting is required
####################################################
$data = <<EOT;
log4j.category = INFO, FileAppndr
log4j.appender.FileAppndr          = Log::Log4perl::Appender::File
log4j.appender.FileAppndr.filename = $testfile
log4j.appender.FileAppndr.mode     = write
log4j.appender.FileAppndr.layout   = Log::Log4perl::Layout::SimpleLayout
EOT

Log::Log4perl::init(\$data);
$log = Log::Log4perl::get_logger("");
$log->info("Shu-wa-chi!");

open FILE, "<$testfile" or die "Cannot create $testfile";
$content = join '', <FILE>;
close FILE;

ok($content, "INFO - Shu-wa-chi!\n");

####################################################
# Explicetly say "append"
####################################################
$data = <<EOT;
log4j.category = INFO, FileAppndr
log4j.appender.FileAppndr          = Log::Log4perl::Appender::File
log4j.appender.FileAppndr.filename = $testfile
log4j.appender.FileAppndr.mode     = append
log4j.appender.FileAppndr.layout   = Log::Log4perl::Layout::SimpleLayout
EOT

Log::Log4perl::init(\$data);
$log = Log::Log4perl::get_logger("");
$log->info("Shu-wa-chi!");

open FILE, "<$testfile" or die "Cannot create $testfile";
$content = join '', <FILE>;
close FILE;

ok($content, "INFO - Shu-wa-chi!\nINFO - Shu-wa-chi!\n");

#########################################################
# Mix Log::Dispatch and Log::Log4perl::Appender appenders
#########################################################
if($LOG_DISPATCH_PRESENT) {
    $data = <<EOT;
log4perl.category = INFO, FileAppndr1, FileAppndr2
log4perl.appender.FileAppndr1          = Log::Dispatch::File
log4perl.appender.FileAppndr1.filename = ${testfile}_1
log4perl.appender.FileAppndr1.mode     = append
log4perl.appender.FileAppndr1.layout   = Log::Log4perl::Layout::SimpleLayout

log4perl.appender.FileAppndr2          = Log::Log4perl::Appender::File
log4perl.appender.FileAppndr2.filename = ${testfile}_2
log4perl.appender.FileAppndr2.mode     = append
log4perl.appender.FileAppndr2.layout   = Log::Log4perl::Layout::SimpleLayout
EOT

    Log::Log4perl::init(\$data);
    $log = Log::Log4perl::get_logger("");
    $log->info("Shu-wa-chi!");

    for(qw(1 2)) {
        open FILE, "<${testfile}_$_" or die "Cannot open ${testfile}_$_";
        $content = join '', <FILE>;
        close FILE;
    
        ok($content, "INFO - Shu-wa-chi!\n");
    }

} else {
    # We don't have Log::Dispatch, skip these cases
    ok(1);
    ok(1);
}

#########################################################
# Check if the 0.33 Log::Log4perl::Appender::File bug is
# fixed which caused all messages to end up in the same 
# file.
#########################################################
$data = <<EOT;
log4perl.category = INFO, FileAppndr1, FileAppndr2
log4perl.appender.FileAppndr1          = Log::Log4perl::Appender::File
log4perl.appender.FileAppndr1.filename = ${testfile}_1
log4perl.appender.FileAppndr1.mode     = write
log4perl.appender.FileAppndr1.layout   = Log::Log4perl::Layout::SimpleLayout

log4perl.appender.FileAppndr2          = Log::Log4perl::Appender::File
log4perl.appender.FileAppndr2.filename = ${testfile}_2
log4perl.appender.FileAppndr2.mode     = write
log4perl.appender.FileAppndr2.layout   = Log::Log4perl::Layout::SimpleLayout
EOT

Log::Log4perl::init(\$data);
$log = Log::Log4perl::get_logger("");
$log->info("Shu-wa-chi!");

for(qw(1 2)) {
    open FILE, "<${testfile}_$_" or die "Cannot open ${testfile}_$_";
    $content = join '', <FILE>;
    close FILE;

    ok($content, "INFO - Shu-wa-chi!\n");
}

#########################################################
# Check if switching over to a new file will work
#########################################################
$data = <<EOT;
log4perl.category = INFO, FileAppndr1
log4perl.appender.FileAppndr1          = Log::Log4perl::Appender::File
log4perl.appender.FileAppndr1.filename = ${testfile}_1
log4perl.appender.FileAppndr1.mode     = write
log4perl.appender.FileAppndr1.layout   = Log::Log4perl::Layout::SimpleLayout
EOT

Log::Log4perl::init(\$data);
$log = Log::Log4perl::get_logger("");
$log->info("File1");

my $app = Log::Log4perl->appenders()->{FileAppndr1};
$app->file_switch("${testfile}_2");
$log->info("File2");

for(qw(1 2)) {
    open FILE, "<${testfile}_$_" or die "Cannot open ${testfile}_$_";
    $content = join '', <FILE>;
    close FILE;

    ok($content, "INFO - File$_\n");
}

ok($app->filename(), "${testfile}_2");

#########################################################
# Testing syswrite
#########################################################
$data = <<EOT;
log4perl.category = INFO, FileAppndr1
log4perl.appender.FileAppndr1          = Log::Log4perl::Appender::File
log4perl.appender.FileAppndr1.filename = ${testfile}_1
log4perl.appender.FileAppndr1.syswrite = 1
log4perl.appender.FileAppndr1.mode     = write
log4perl.appender.FileAppndr1.layout   = Log::Log4perl::Layout::SimpleLayout
EOT

Log::Log4perl::init(\$data);
$log = Log::Log4perl::get_logger("");
$log->info("File1");

open FILE, "<${testfile}_1" or die "Cannot open ${testfile}_1";
$content = join '', <FILE>;
close FILE;

ok($content, "INFO - File1\n");

Log::Log4perl::init(\$data);
$log->info("File1");

open FILE, "<${testfile}_1" or die "Cannot open ${testfile}_1";
$content = join '', <FILE>;
close FILE;

ok($content, "INFO - File1\n");

#########################################################
# Testing syswrite with append
#########################################################
$data = <<EOT;
log4perl.category = INFO, FileAppndr1
log4perl.appender.FileAppndr1          = Log::Log4perl::Appender::File
log4perl.appender.FileAppndr1.filename = ${testfile}_1
log4perl.appender.FileAppndr1.syswrite = 1
log4perl.appender.FileAppndr1.mode     = append
log4perl.appender.FileAppndr1.layout   = Log::Log4perl::Layout::SimpleLayout
EOT

Log::Log4perl::init(\$data);
$log = Log::Log4perl::get_logger("");
$log->info("File1");

open FILE, "<${testfile}_1" or die "Cannot open ${testfile}_1";
$content = join '', <FILE>;
close FILE;

ok($content, "INFO - File1\nINFO - File1\n");

#########################################################
# Testing create_at_logtime
#########################################################
$data = qq(
log4perl.category         = DEBUG, Logfile
log4perl.appender.Logfile          = Log::Log4perl::Appender::File
log4perl.appender.Logfile.filename = ${testfile}_3
log4perl.appender.Logfile.create_at_logtime = 1
log4perl.appender.Logfile.layout   = Log::Log4perl::Layout::SimpleLayout
);

Log::Log4perl->init(\$data);
ok(! -f "${testfile}_3");

$log = Log::Log4perl::get_logger("");
$log->info("File1");

open FILE, "<${testfile}_3" or die "Cannot open ${testfile}_3";
$content = join '', <FILE>;
close FILE;

ok($content, "INFO - File1\n");
