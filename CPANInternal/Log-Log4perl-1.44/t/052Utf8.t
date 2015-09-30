###########################################
# Test Suite for utf8 output
# Mike Schilli, 2004 (m@perlmeister.com)
###########################################

BEGIN { 
    if($ENV{INTERNAL_DEBUG}) {
        require Log::Log4perl::InternalDebug;
        Log::Log4perl::InternalDebug->enable();
    }
}

use strict;

my $EG_DIR = "eg";
$EG_DIR = "../eg" unless -d $EG_DIR;

use Test::More;
use Log::Log4perl qw(:easy);

BEGIN {
    if($] < 5.008) {
        plan skip_all => "utf-8 tests with perl >= 5.8 only";
    } else {
        plan tests => 6;
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

###########
# Easy mode with utf8 setting
###########

open STDERR, ">$TMP_FILE";
select STDERR; $| = 1; #needed on win32
select STDOUT;
open IN, "<$TMP_FILE" or die "Cannot open $TMP_FILE"; binmode IN, ":utf8";
sub readstderr { return join("", <IN>); }

END   { unlink $TMP_FILE;
        close IN;
      }

Log::Log4perl->easy_init({
    level => $DEBUG,
    file  => "STDERR",
    utf8  => 1,
});

use utf8;
DEBUG "Über";
binmode STDOUT, ":utf8"; # for better error messages of the test suite
like(readstderr(), qr/Über/, 'utf8 matches');

###########
# utf8 config file
###########
use Log::Log4perl::Config;
Log::Log4perl::Config->utf8(1);
Log::Log4perl->init("$EG_DIR/log4j-utf8.conf");
DEBUG "blech";
my $app = Log::Log4perl::Appender::TestBuffer->by_name("Ä1");
ok defined $app, "app found";
my $buf = $app->buffer();
is $buf, "blech\n", "utf8 named appender";
