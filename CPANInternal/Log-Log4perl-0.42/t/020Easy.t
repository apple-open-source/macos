# Tests for the lazy man:s logger with easy_init()

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

BEGIN { plan tests => 9 }

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

is($data, "020Easy.t-67-Bar::Mars::crunch: Mars mjam\nTwix mjam\n");

############################################################
# LOGDIE and LOGWARN
############################################################
# redir STDERR again
open STDERR, ">$TMP_FILE";
select STDERR; $| = 1; #needed on win32
open IN, "<$TMP_FILE" or die "Cannot open $TMP_FILE";

Log::Log4perl->easy_init($INFO);
$log = get_logger();
eval { LOGDIE("logdie"); };

like($@, qr/logdie at .*?020Easy.t line 103/);
like(readstderr(), qr/^[\d:\/ ]+logdie$/);

LOGWARN("logwarn");
like(readstderr(), qr/logwarn/);

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

############################################################
# Finally close
############################################################
close IN;
