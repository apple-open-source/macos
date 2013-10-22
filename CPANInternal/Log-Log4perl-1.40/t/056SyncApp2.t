#!/usr/bin/perl
##########################################################################
# The test checks Log::Log4perl::Appender::Synchronized for correct semaphore
# destruction when using parameter "destroy".
# Based on: 042SyncApp.t
# Jens Berthold, 2009 (log4perl@jebecs.de)
##########################################################################
use warnings;
use strict;

BEGIN { 
    if($ENV{INTERNAL_DEBUG}) {
        require Log::Log4perl::InternalDebug;
        Log::Log4perl::InternalDebug->enable();
    }
}

use Test::More;
use Log::Log4perl qw(:easy);
Log::Log4perl->easy_init($DEBUG);
use constant INTERNAL_DEBUG => 0;

our $INTERNAL_DEBUG = 0;

$| = 1;

BEGIN {
    if(exists $ENV{"L4P_ALL_TESTS"}) {
        plan tests => 1;
    } else {
        plan skip_all => "- only with L4P_ALL_TESTS";
    }
}

use Log::Log4perl::Util::Semaphore;
use Log::Log4perl qw(get_logger);
use Log::Log4perl::Appender::Synchronized;

my $EG_DIR = "eg";
$EG_DIR = "../eg" unless -d $EG_DIR;

my $logfile = "$EG_DIR/fork.log";

our $lock;

unlink $logfile;

my $conf = qq(
log4perl.category.Bar.Twix          = WARN, Syncer

log4perl.appender.Logfile           = Log::Log4perl::Appender::TestFileCreeper
log4perl.appender.Logfile.autoflush = 1
log4perl.appender.Logfile.filename  = $logfile
log4perl.appender.Logfile.layout    = Log::Log4perl::Layout::PatternLayout
log4perl.appender.Logfile.layout.ConversionPattern = %F{1}%L> %m%n

log4perl.appender.Syncer           = Log::Log4perl::Appender::Synchronized
log4perl.appender.Syncer.appender  = Logfile
log4perl.appender.Syncer.key       = blah
log4perl.appender.Syncer.destroy   = 1
);

Log::Log4perl::init(\$conf);

my $pid = fork();

die "fork failed" unless defined $pid;

my $logger = get_logger("Bar::Twix");
if($pid) {
   # parent
   # no logging test here: if child erroneously deletes semaphore, 
   # any log output at this point would crash the test 
} else { 
   # child
   exit 0;
}

# Wait for child to finish
print "Waiting for pid $pid\n" if $INTERNAL_DEBUG;
waitpid($pid, 0);
print "Done waiting for pid $pid\n" if $INTERNAL_DEBUG;
unlink $logfile;

# Destroying appender (+semaphore) fails if child process already destroyed it
Log::Log4perl->appender_by_name('Syncer')->DESTROY();
ok(!$@, "Destroying appender");

