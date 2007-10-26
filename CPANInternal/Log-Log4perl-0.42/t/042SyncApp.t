#!/usr/bin/perl
##########################################################################
# Synchronizing appender output with Log::Log4perl::Appender::Synchronized.
# This test uses fork and a semaphore to get two appenders to get into
# each other/s way.
# Mike Schilli, 2003 (m@perlmeister.com)
##########################################################################
use warnings;
use strict;

use Test::More;

$| = 1;

BEGIN {
    if(exists $ENV{"L4P_ALL_TESTS"}) {
        plan tests => 2;
    } else {
        plan skip_all => "- only with L4P_ALL_TESTS";
    }
}

use IPC::Shareable qw(:lock);
use Log::Log4perl qw(get_logger);
use Log::Log4perl::Appender::Synchronized;

my $EG_DIR = "eg";
$EG_DIR = "../eg" unless -d $EG_DIR;

my $logfile = "$EG_DIR/fork.log";

our $lock;
our $locker;
our $shared_name = "_l4_";

#print "Nuking semaphore\n";
Log::Log4perl::Appender::Synchronized::nuke_sem($shared_name);
Log::Log4perl::Appender::Synchronized::nuke_sem("_l4p");

unlink $logfile;

#goto SECOND;

#print "tie\n";
$locker = tie $lock, 'IPC::Shareable', $shared_name, 
    { create  => 1, 
      destroy => 1} or
    die "Cannot create shareable $shared_name";

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
);

$locker->shunlock();
$locker->shlock();

Log::Log4perl::init(\$conf);

my $pid = fork();

die "fork failed" unless defined $pid;

my $logger = get_logger("Bar::Twix");
if($pid) {
   #parent
   $locker->shlock();
   #print "Waiting for child\n";
   for(1..10) {
       #print "Parent: Writing\n";
       $logger->error("X" x 4097);
   }
} else { 
   #child
   $locker->shunlock();
   for(1..10) {
       #print "Child: Writing\n";
       $logger->error("Y" x 4097);
   }
   exit 0;
}

   # Wait for child to finish
waitpid($pid, 0);

my $clashes_found = 0;

open FILE, "<$logfile" or die "Cannot open $logfile";
while(<FILE>) {
    if(/XY/ || /YX/) {
        $clashes_found = 1;
        last;
    }
}
close FILE;

unlink $logfile;
#print $logfile, "\n";
#exit 0;

$locker->clean_up;

ok(! $clashes_found, "Checking for clashes in logfile");

###################################################################
# Test the Socket appender
###################################################################

use IO::Socket::INET;

SECOND:

#print "Nuking semaphore\n";
Log::Log4perl::Appender::Synchronized::nuke_sem($shared_name);
Log::Log4perl::Appender::Synchronized::nuke_sem("_l4p");

unlink $logfile;

#print "tie\n";
$locker = tie $lock, 'IPC::Shareable', $shared_name, 
    { create  => 1, 
      destroy => 1} or
    die "Cannot create shareable $shared_name";

$conf = q{
    log4perl.category                  = WARN, Socket
    log4perl.appender.Socket           = Log::Log4perl::Appender::Socket
    log4perl.appender.Socket.PeerAddr  = localhost
    log4perl.appender.Socket.PeerPort  = 12345
    log4perl.appender.Socket.layout    = SimpleLayout
};

#print "unlock\n";
$locker->shunlock();
#print "lock\n";
$locker->shlock();

#print "forking\n";
$pid = fork();

die "fork failed" unless defined $pid;

if($pid) {
   #parent
   #print "Waiting for child\n";
   $locker->shlock();

   {
       my $client = IO::Socket::INET->new( PeerAddr => 'localhost',
                                           PeerPort => 12345,
                                         );

       #print "Checking connection\n";

       if(defined $client) {
           #print "Client defined, sending test\n";
           eval { $client->send("test\n") };
           if($@) {
               #print "Send failed ($!), retrying ...\n";
               sleep(1);
               redo;
           }
       } else {
           #print  "Server not responding yet ($!) ... retrying\n";
           sleep(1);
           redo;
       }
       $client->close();
   }

   #print "Done\n";

   Log::Log4perl::init(\$conf);
   $logger = get_logger("Bar::Twix");
   #print "Sending message\n";
   $logger->error("Greetings from the client");
} else { 
   #child

   #print STDERR "child starting\n";
   my $sock = IO::Socket::INET->new(
       Listen    => 5,
       LocalAddr => 'localhost',
       LocalPort => 12345,
       ReuseAddr => 1,
       Proto     => 'tcp');

   die "Cannot start server: $!" unless defined $sock;
       # Ready to receive
   #print "Server started\n";
   $locker->shunlock();

   my $nof_messages = 2;

   open FILE, ">$logfile" or die "Cannot open $logfile";
   while(my $client = $sock->accept()) {
       #print "Client connected\n";
       while(<$client>) {
           print FILE "$_\n";
           last;
       }
       last unless --$nof_messages;
   }

   close FILE;
   exit 0;
}

   # Wait for child to finish
waitpid($pid, 0);

open FILE, "<$logfile" or die "Cannot open $logfile";
my $data = join '', <FILE>;
close FILE;

unlink $logfile;

like($data, qr/Greetings/, "Check logfile of Socket appender");
