###########################################
# Test Suite for utf8 output
# Mike Schilli, 2004 (m@perlmeister.com)
###########################################


use strict;

use Test::More;
use Log::Log4perl qw(:easy);

BEGIN {
    if($] < 5.008) {
        plan skip_all => "utf-8 tests with perl >= 5.8 only";
    } else {
        plan tests => 3;
    }
}

my $WORK_DIR = "tmp";
if(-d "t") {
    $WORK_DIR = File::Spec->catfile(qw(t tmp));
}
unless (-e "$WORK_DIR"){
    mkdir("$WORK_DIR", 0755) || die "can't create $WORK_DIR ($!)";
}

my $TMP_FILE = File::Spec->catfile(qw(t tmp utf8.out));
$TMP_FILE = "tmp/utf8.out" if ! -d "t";

END   {
        unlink $TMP_FILE;
        rmdir $WORK_DIR;
      }

###########
# utf8 file appender
###########
my $conf = <<EOT;
    log4perl.logger = DEBUG, A1
    log4perl.appender.A1=Log::Log4perl::Appender::File
    log4perl.appender.A1.filename=$TMP_FILE
    log4perl.appender.A1.mode=write
    log4perl.appender.A1.utf8=1
    log4perl.appender.A1.layout=PatternLayout
    log4perl.appender.A1.layout.ConversionPattern=%d-%c %m%n
EOT
Log::Log4perl->init(\$conf);
DEBUG "quack \x{A4}";
open FILE, "<:utf8", $TMP_FILE or die "Cannot open $TMP_FILE";
my $data = join '', <FILE>;
close FILE;
like($data, qr/\x{A4}/, "conf: utf8-1");

###########
# binmode
###########
$conf = <<EOT;
    log4perl.logger = DEBUG, A1
    log4perl.appender.A1=Log::Log4perl::Appender::File
    log4perl.appender.A1.filename=$TMP_FILE
    log4perl.appender.A1.mode=write
    log4perl.appender.A1.binmode=:utf8
    log4perl.appender.A1.layout=PatternLayout
    log4perl.appender.A1.layout.ConversionPattern=%d-%c %m%n
EOT
Log::Log4perl->init(\$conf);
DEBUG "quack \x{A5}";
open FILE, "<:utf8", $TMP_FILE or die "Cannot open $TMP_FILE";
$data = join '', <FILE>;
close FILE;
like($data, qr/\x{A5}/, "binmode: utf8-1");

###########
# Easy mode
###########
Log::Log4perl->easy_init({file  => ":utf8> $TMP_FILE",
                          level => $DEBUG});

DEBUG "odd character: \x{30B8}";
open FILE, "<:utf8", $TMP_FILE or die "Cannot open $TMP_FILE";
$data = join '', <FILE>;
close FILE;
like($data, qr/\x{30B8}/, "easy: utf8-1");


