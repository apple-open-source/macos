#!/usr/bin/perl
# This is script will connect to your isp (if not already connected),
#   send any outgoing mail and retrieve any incoming mail.  If this
#   program made the connection, it will also break the connection
#   when it is done.
#
# Bill Adams
# bill@evil.inetarena.com
#
# Revision History
# 1.0.1   05 Sep 1998  baa  Massive updates to work with fetchmail.
# 
# Get the latest version from my home-page:
#  http://www.inetarena.com/~badams/computerstuff.html
#  following the 'Stuff I Have Written' link.
#
# License: GNU, but tell me of any improvements or changes.
#
use strict;

my $suck;
my $rdate;
my ($my_syslog, $debug, $verbose);

my $start_time = time;
my $mailhost = 'mail';
my $sendmail_queue_dir = '/var/spool/mqueue/';	#Need trailing slash!
my $interface = 'ppp0';         #Watch this interface
my $max_tries = 1;		#How many times to try and re-dial
my $retry_delay = 300;	        #How long to wait to retry (in seconds)
my $connect_timeout = 45;	#How long to wait for connection

#For the log file, be sure to put the >, >>, or | depending on
#  what you want it to do.  I have also written a little program
#  called simple_syslog that you can pipe the data to.
my $log_file = '>/dev/null';    #Where to put the data.
$log_file = '>>/var/log/mailqueue.pl';
#$log_file = '>/dev/console';

my $this_hour = +[localtime()]->[2];

#Define this to get mail between midnight and 5 am
#$suck = '/var/spool/suck/get.news.inn';

#Define this to set the time to a remote server
#$rdate = '/usr/bin/rdate -s clock1.unc.edu';

#Where are the programs are located.  You can specify the full path if needed.
my $pppd = 'pppd';
my $fetchmail = 'fetchmail'; #'etrn.pl';
my $sendmail = 'sendmail';

#Where is the etrn/fetchmail pid
my $fetchmail_pid = '/var/run/fetchmail.pid';


#Set the path to where we think everything will live.
$ENV{'PATH'} = ":/bin:/usr/bin:/sbin:/usr/sbin:/usr/local/bin:";
my $lockfile = "/var/run/mailqueue.lock";	#lockfile for this program
my $space = ' ';			#Never know when you might need space
my $program_name = $0;
$program_name = substr ($program_name, (rindex ($program_name, '/') + 1));
open SYSLOG, $log_file or die "Could not open $log_file\n\t";

sys_log ("Started by UID $<");
#$< = 0;					#suid root

#Other global vars
my $pppd_pid;

#Make sure we are root.  This has to be the case for everything
#  to work properly.
if ($< != 0) {
    sys_log ("Not root...exit");
    print STDERR "You are not root...sorry cannot run.\n";
    exit (1);
}

sub sys_log {
    #Writes a message to the log file.
    my ($message) = @_;
    print SYSLOG join(' ', 
		      $program_name,
		      ''.localtime(), #The '' puts it in a scaler context.
		      $message)."\n";
    print STDERR $message, "\n" if $debug;
}

#Get the command line args.
$verbose = 1;
for (my $i = 0; $i <= $#ARGV; $i++) {
    if ($ARGV[$i] eq '-v' || $ARGV[$i] eq '-verbose') {
	$verbose++;
	print "Running in verbose mode level ($verbose).\n";
    } elsif ($ARGV[$i] eq '-d' || $ARGV[$i] eq '-debug') {
	$debug++;
	$verbose = 10;	#Some high value so everything gets printed
	print STDERR "Running in debug mode.\n";
    } elsif ($ARGV[$i] eq '-q' || $ARGV[$i] eq '-quiet') {
	$debug = 0;
	$verbose = 0; 
    } elsif ($ARGV[$i] eq '-max_tries') {
	if (not defined $ARGV[$i + 1]) {
	    printf STDERR "$0: Error: option -max_tries requires a value.\n";
	    &usage;
	} else {
	    $max_tries = $ARGV[$i + 1];
	}
    } elsif ($ARGV[$i] eq '-retry_delay') {
	if (not defined $ARGV[$i + 1]) {
	    printf STDERR "$0: Error: option -retry_delay requires a value.\n";
	    &usage;
	} else {
	    $max_tries = $ARGV[$i + 1];
	}
    } elsif ($ARGV[$i] eq '-interface') {
	if (not defined $ARGV[$i + 1]) {
	    printf STDERR "$0: Error: option -interface requires a value.\n";
	    &usage;
	} else {
	    $max_tries = $ARGV[$i + 1];
	}
    } elsif ($ARGV[$i] eq '-mailhost') {
	if (not defined $ARGV[$i + 1]) {
	    printf STDERR "$0: Error: option -mailhost requires a value.\n";
	    &usage;
	} else {
	    $mailhost = $ARGV[$i + 1];
	}
    } else {
	print STDERR "Unknown command line option: [". $ARGV[$i]."]\n";
	&usage;
    }
}


$| = 1 if $verbose;			#Output un-buffered if we are verbose


#Do some checking for programs
&check_program ($my_syslog) || die "$0 -> Error: $my_syslog is required\n";
($fetchmail = &check_program ($fetchmail)) 
    || die "$0 -> Error: Could not find fetchmail/etrn\n";
($pppd = &check_program ($pppd))
    || die "$0 -> Error: Could not find pppd\n";
(-d $sendmail_queue_dir) || die "$0 -> Error: The sendmail queue directory\n\t[$sendmail_queue_dir] does not exist or is not a directory.\n";
($sendmail = &check_program ($sendmail)) 
    || die "$0 -> Error: Could not find $sendmail\n";


#Do some process locking.  This kills any already running processes.
if (-s $lockfile) {
    my $pid = `cat $lockfile`; chop $pid;
    if (not &process_is_dead ($pid)) {
	print STDERR "$0 -> Process locked by pid $pid killing it.\n" 
	    if $verbose;
	kill 15, $pid;
	waitpid ($pid, 0); #This has no effect.
    }
    sys_log ("Removing stale lock for pid $pid") if $verbose;
    unlink ($lockfile) || die $!;
}
open (LOCK, '>'.$lockfile) || die "$0: Could not create lockfile $lockfile\n";
print LOCK $$, "\n";
close LOCK;

#print out some info if needed.
if ($debug) {
    print STDERR "             Max tries: $max_tries\n";
    print STDERR "      Dial Retry Delay: $retry_delay seconds.\n";
    print STDERR "Interface set to watch: $interface\n";
    print STDERR " Mailhost set to watch: $mailhost\n";
    print STDERR "    Connection timeout: $connect_timeout\n";
    print STDERR "              Sendmail: $sendmail\n";
    print STDERR "                  pppd: $pppd\n";
    print STDERR "     fetchmail/etrn.pl: $fetchmail\n";
    print STDERR "\n\n";
}
((-x $pppd) && (-x $sendmail) && (-x $fetchmail)) 
	|| die "Still some problem with programs.\n\tRun with -d to see if the path is specified for sendmail,\n\tpppd and fetchmail/etrn.pl";

while ($max_tries--) {
    my $child_pid;
    unless ($child_pid = fork)  {
	#This is the child process that waits for a connection to be made
	#  and then sends the local mail queue and then sends a request to
	#  get the remote mail queue
	my $count = $connect_timeout;
	while (&interface_is_down ($interface) && $count--) {sleep (1)}
	if ($count < 1) {exit (1)}
	
	#Send any queued mail.  I had another routine that would
	#  fork and watch sendmail with a timeout, but that is kinda
	#  flaky depending on how big your queue size is. So
	#  now just call it and wait for it to return.  If you have bad
	#  messages in your queue, this can hang.
	sys_log ("Have connection->sending any local mail.") if $verbose;
	system("$sendmail -q");

	sys_log ("Checking remote queue on ($mailhost)");

	my $result;
	my $daemon = 0;
	my $pid;
	#In case we have a pid, read it and find out if it is
	#  still valid or not.
	if (defined $fetchmail_pid and -f $fetchmail_pid) {
	    if (not open PID, $fetchmail_pid) {
		sys_log("Could not open $fetchmail_pid");
		die}
	    $pid = <PID>;
	    if ($pid =~ m|([0-9]+)\s+([0-9]*)|) {
		$pid = $1;
		$daemon = $2;
	    }
	    close PID;
	    sys_log("Have PID file ($fetchmail_pid) with PID $pid $daemon");
	    #In the case of fetchmail, we need to see if it is
	    #  still running in case there is a stale lock file.
	    if (&process_is_dead($pid)) {
		sys_log("  It is no longer running");
		$daemon = 0; $pid = 0}
	}
	if (not $pid or ($pid and $daemon)) {
	    #Either it is not running or it is running and a daemon.
	    sys_log("Running $fetchmail [$daemon]");
	    my $result = (system ($fetchmail))/256;
	    sys_log($fetchmail.' exited with status '.$result) if $debug;
	} else {
	    sys_log("$fetchmail already running...");
	}

	#Watch the directory for n seconds of inactivity.
	sys_log("Fetchmail done...watching $sendmail_queue_dir");
	&watch_dir ($sendmail_queue_dir, 10);
	sys_log ("Done polling for mail");
	
	if (-f $fetchmail_pid and not $daemon) {
	    #In case something went wrong and the fetchmail is still 
	    # running (and not a daemon)....
	    my $result = `$fetchmail -q`; chop $result;
	    sys_log($result);
	}
	exit (0);
    }
    #If a connection is needed, make it.
    if (&interface_is_down ($interface) && $pppd_pid == 0) {
	sys_log ("Try to connect with pppd") if $debug;
	# Fork pppd with a pid we can track.
	unless ($pppd_pid = fork) {
		exec ($pppd.' -detach');
	}
    }
    #Wait for the child to exit and check for errors
    waitpid ($child_pid, 0);
    my $child_status = ($? / 256);
    my $child_kill = $? % 256;
    if ($child_status == 0) {
	if ($this_hour <= 4 and defined $suck) {
	    sys_log ("Calling suck...");
	    print `$suck`;
	}
	if (defined $rdate) {
	   sys_log ("Calling rtime...");
	   print `$rdate`;
	}
	if ($pppd_pid) { 	#If we ran pppd, kill it
	    sys_log ("Killing pppd (pid $pppd_pid)");
	    kill 15, $pppd_pid;
	    waitpid ($pppd_pid, 0);	#Wait for clean exit of child
	}
	sys_log ("Finished with cycle.");
	unlink ($lockfile);
	sys_log ("Total time: ".(time-$start_time)." seconds") if $debug;
	exit (0);
    }
    # Reset to pppp_pid to zero if pppd is not running.
    if ($pppd_pid && &process_is_dead ($pppd_pid)) {$pppd_pid = 0}
    sys_log (join ('', "Warn: Did not connect -> Try ",
		   $max_tries, " more times...after ",
		   $retry_delay, " seconds"));
    if (not $max_tries) {
	sys_log ("Giving up...");
	exit (1);
    }
    sleep ($retry_delay);
    sys_log ("ok...trying again.");
}

sub check_program {
    #See if a program is in the path
    my ($program) = @_;
    my $exists = 0;
    my $path_specified = 0;
    my $path;
    
    #catch the case where there is already a slash in the argument.

    if ($program =~ /\//) {
	$path_specified = 1;
	if (-x $program) {$exists = $program}
    }

    my $exists;
    foreach $path (split(/:/, $ENV{'PATH'})) {
	next if length ($path) < 3;		#skip bogus path entries
	#be sure the there is a trailing slash
	if (substr ($path, -1, 1) ne '/') {$path .= '/'}
	#Check to see if it exists and is executable
	if (-x $path.$program) {$exists = $path.$program; last}
    }
    if (not $exists) {
	if ($path_specified) {
	    print STDERR "$0 -> Warn: ". $program. 
		" is not executable or does not exist.\n";
	} else {
	    print STDERR "$0 -> Warn: [$program] was not found in path\n\t".
		$ENV{'PATH'}."\n";
	}
    }
    return ($exists);
}


sub process_is_dead {
    #This is a cheap way to check for running processes.  I could use
    #  the /proc file-system in Linux but that would not be very 
    #  friendly to other OS's.
    #
    #return 1 if pid is not in process list
    # This expects ps to return a header line and then another line if
    #  the process is running.  Also check for zombies
    my ($pid) = @_;
    my @results = split (/\n/, `ps $pid 2>/dev/null`);
    if (not defined $results[1]) {return  1}
    if ($results[1] =~ /zombie/i) {return 1}
    return 0;
}


sub interface_is_down {
    # return 1 (true) if the ip is down
    my ($interface) = @_;
    if (`ifconfig $interface` =~ /UP/) {
	return 0;
    } else {
	return 1;
    }
}

sub watch_dir {
    #Watch the mailqueue directory for incoming files.
    #  The 'xf' files are the transfer (xfer) files on my system.
    #  If you find this is not the case, please email me.  To be safe,
    #  I check the latest mod time as long as xf files exist.  If no
    #  data has made it over in n seconds, we will assume that an
    #  error has occured and give up.

    my $files_like = '^(xf.*)'; #Regexp
    my $dir_to_watch = shift;
    my $delay = shift;
    my $timeout = 120;  #Give it 120 seconds to get data.
    my $loop_delay = 1; #How long between each loop. Do not make 0!

    #Make sure there is a trailing slash.
    if ($dir_to_watch !~ m|/$|) {$dir_to_watch .= '/'}

    #How long to wait for transfer of data.  This gets reset
    #  each time the mod time falls below a certain time.
    my $flag = $delay;	
    my $last_total = 0;
    
    while (($flag -= $loop_delay) > 0) {
	sleep $loop_delay;
	opendir (DIR, $dir_to_watch);
	my $file_count = 0;
	my $last_data_dl = 500000; #Big Number
	foreach my $file (readdir (DIR)) {
	    next if not -f $dir_to_watch.$file; #Only files.
	    my @stats = stat($dir_to_watch.$file);
	    my $m_time = time - $stats[9];
	    #Here, if we have a recent file, reset the timeout.
	    if ($m_time < $last_data_dl) {$last_data_dl = $m_time}
	    
	    #If we have an xfer file, up the delay.
	    if ($file =~ m|$files_like|) {
		sys_log("$file is like $files_like");
		$flag = $delay;
	    }
	}
	closedir (DIR);
	sys_log ("Watch_dir: $flag ($last_data_dl)") if $debug;

	#In the case of now data downloaded...
	if ($last_data_dl > $timeout and $flag == $delay) {
	    sys_log("Watch_dir: Timed out after $timeout seconds.");
	    $flag = 0;
	}
    }
    sys_log ("Watch_dir: Done.");
}

sub usage {
    #print the usage
    print join ("\n",
		'mailqueue.pl -- A program to send and receive mail form a sendmail spooler.',
		'  Requires that you ISP is running sendmail, at lease version 8.6.?.',
		'  Also requires that you have fetchmail or etrn.pl installed on this system.',
		'', 'Command line args (Default in parrens):',
		'  -v -verbose         Run in verbose mode.  Can use this arg multiple times.',
		'  -d -debug           Run in debug mode.  Sets verbose level to 10',
		'  -max_tries N	       Sets the maximum number of connect retries to N.  ('.$max_tries.')',
		'  -retry_delay N      Sets the delay between retrying to N seconds.  ('. $retry_delay.')',
	        "  -connect_timeout N  Sets the connection timeout to N seconds. (". $connect_timeout. ')',
		'  -interface STR      Sets the default interface to STR.  ('. $interface. ')',
		'  -mailhost STR       Sets the mailhost to STR. ('. $mailhost.')',
		'');
    exit (1);
}

