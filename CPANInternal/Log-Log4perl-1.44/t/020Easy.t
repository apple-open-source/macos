# Tests for the lazy man:s logger with easy_init()

BEGIN { 
    if($ENV{INTERNAL_DEBUG}) {
        require Log::Log4perl::InternalDebug;
        Log::Log4perl::InternalDebug->enable();
    }
}

use warnings;
use strict;

use Test::More;
use Log::Log4perl qw(:easy);
use File::Spec;

my $WORK_DIR = "tmp";
if(-d "t") {
    $WORK_DIR = File::Spec->catfile(qw(t tmp));
}
unless (-e "$WORK_DIR"){
    mkdir("$WORK_DIR", 0755) || die "can't create $WORK_DIR ($!)";
}

my $TMP_FILE = File::Spec->catfile(qw(t tmp easy));
$TMP_FILE = "tmp/easy" if ! -d "t";

BEGIN {
    if ($] < 5.006) {
        plan skip_all => "Only with perl >= 5.006";
    } else {
        plan tests => 21;
    }
}

END   { unlink $TMP_FILE;
        close IN;
      }

ok(1); # Initialized ok
unlink $TMP_FILE;

# Capture STDOUT to a temporary file and a filehandle to read from it
open STDERR, ">$TMP_FILE";
select STDERR; $| = 1; #needed on win32
open IN, "<$TMP_FILE" or die "Cannot open $TMP_FILE";
sub readstderr { return join("", <IN>); }

############################################################
# Typical easy setup
############################################################
Log::Log4perl->easy_init($INFO);
my $log = get_logger();
$log->debug("We don't want to see this");
$log->info("But this we want to see");
$log->error("And this also");
my $stderr = readstderr();
#print "STDERR='$stderr'\n";

unlike($stderr, qr/don't/);
like($stderr, qr/this we want/);
like($stderr, qr/this also/);

############################################################
# Advanced easy setup
############################################################
Log::Log4perl->reset();
close IN;
    # Reopen stderr
open STDERR, ">&1";
unlink $TMP_FILE;

package Bar::Twix;
use Log::Log4perl qw(:easy);
sub crunch { DEBUG("Twix Not shown"); 
             ERROR("Twix mjam"); }

package Bar::Mars;
use Log::Log4perl qw(:easy);
my $line = __LINE__ + 1;
sub crunch { ERROR("Mars mjam"); 
             INFO("Mars not shown"); }
package main;

Log::Log4perl->easy_init(
         { level    => $INFO,
           category => "Bar::Twix",
           file     => ">>$TMP_FILE",
           layout   => '%m%n',
         },
         { level    => $WARN,
           category => "Bar::Mars",
           file     => ">>$TMP_FILE",
           layout   => '%F{1}-%L-%M: %m%n',
         },
);

Bar::Mars::crunch();
Bar::Twix::crunch();

open FILE, "<$TMP_FILE" or die "Cannot open $TMP_FILE";
my $data = join '', <FILE>;
close FILE;

is($data, "020Easy.t-$line-Bar::Mars::crunch: Mars mjam\nTwix mjam\n");

############################################################
# LOGDIE and LOGWARN
############################################################
# redir STDERR again
open STDERR, ">$TMP_FILE";
select STDERR; $| = 1; #needed on win32
open IN, "<$TMP_FILE" or die "Cannot open $TMP_FILE";

Log::Log4perl->easy_init($INFO);
$log = get_logger();
$line = __LINE__ + 1;
eval { LOGDIE("logdie"); };

like($@, qr/logdie at .*?020Easy.t line $line/);
like(readstderr(), qr/^[\d:\/ ]+logdie$/m);

LOGWARN("logwarn");
like(readstderr(), qr/logwarn/);

############################################################
# Test logdie/logwarn with and without "\n"s
############################################################
LOGWARN("message");
like(readstderr(), qr/message at .*? line \d+/);

LOGWARN("message\n");
unlike(readstderr(), qr/message at .*? line \d+/);

LOGWARN("message\nother");
like(readstderr(), qr/other at .*? line \d+/);

LOGWARN("message\nother\n");
unlike(readstderr(), qr/other at .*? line \d+/);

    # logdie
eval { LOGDIE("logdie"); };
like($@, qr/logdie at .*?020Easy.t line \d+/);

eval { LOGDIE("logdie\n"); };
unlike($@, qr/at .*?020Easy.t line \d+/);

eval { LOGDIE("logdie\nother"); };
like($@, qr/other at .*?020Easy.t line \d+/);

eval { LOGDIE("logdie\nother\n"); };
unlike($@, qr/at .*?020Easy.t line \d+/);

############################################################
# Test %T stack traces
############################################################
Log::Log4perl->easy_init({ level => $INFO, layout => "%T: %m%n"});

sub foo {
   bar();
}

sub bar {
    my $log = get_logger();
    $log->info("info!");
}

foo();
like(readstderr(), qr(main::bar.*?main::foo));
close IN;

############################################################
# LOGCARP and LOGCROAK
############################################################
# redir STDERR again
open STDERR, ">$TMP_FILE";
select STDERR; $| = 1; #needed on win32
open IN, "<$TMP_FILE" or die "Cannot open $TMP_FILE";

package Whack;
use Log::Log4perl qw(:easy);
sub whack {
    LOGCROAK("logcroak in whack");
}

package main;

Log::Log4perl->easy_init($INFO);
$log = get_logger();
$line = __LINE__ + 1;
eval { Whack::whack() };

like($@, qr/logcroak in whack at .*?020Easy.t line $line/);
like(readstderr(), qr/^[\d:\/ ]+logcroak in whack.*$line/m);

$line = __LINE__ + 8;
package Junk1;
use Log::Log4perl qw(:easy);
sub foo {
    LOGCARP("LOGCARP");
}
package Junk2;
sub foo {
    Junk1::foo();
}
package main;
Junk2::foo();
SKIP: {
    use Carp; 
    skip "Detected buggy Carp.pm (upgrade to perl-5.8.*)", 1 unless 
        defined $Carp::VERSION;
    like(readstderr(), qr/LOGCARP.*020Easy.t line $line/);
}

############################################################
# LOGDIE and wrapper packages
############################################################
package JunkWrapper;
use Log::Log4perl qw(:easy);
sub foo {
    LOGDIE("Ahhh");
}

package main;

Log::Log4perl->wrapper_register("JunkWrapper");
$line = __LINE__ + 2;
eval {
    JunkWrapper::foo();
};
like $@, qr/line $line/, "logdie with wrapper";

# Finally close
############################################################
close IN;
