# -*- perl -*-
#
#   $Id: Daemon.pm,v 1.3 1999/09/26 14:50:12 joe Exp $
#
#   Net::Daemon - Base class for implementing TCP/IP daemons
#
#   Copyright (C) 1998, Jochen Wiedmann
#                       Am Eisteich 9
#                       72555 Metzingen
#                       Germany
#
#                       Phone: +49 7123 14887
#                       Email: joe@ispsoft.de
#
#   All rights reserved.
#
#   You may distribute this package under the terms of either the GNU
#   General Public License or the Artistic License, as specified in the
#   Perl README file.
#
############################################################################

require 5.004;
use strict;

use Getopt::Long ();
use Symbol ();
use IO::Socket ();
use Config ();
use Net::Daemon::Log ();
use POSIX ();


package Net::Daemon;

$Net::Daemon::VERSION = '0.43';
@Net::Daemon::ISA = qw(Net::Daemon::Log);

#
#   Regexps aren't thread safe, as of 5.00502 :-( (See the test script
#   regexp-threads.)
#
$Net::Daemon::RegExpLock = 1;

use vars qw($exit);

############################################################################
#
#   Name:    Options (Class method)
#
#   Purpose: Returns a hash ref of command line options
#
#   Inputs:  $class - This class
#
#   Result:  Options array; any option is represented by a hash ref;
#            used keys are 'template', a string suitable for describing
#            the option to Getopt::Long::GetOptions and 'description',
#            a string for the Usage message
#
############################################################################

sub Options ($) {
    { 'catchint' => { 'template' => 'catchint!',
		      'description' => '--nocatchint            '
		          . "Try to catch interrupts when calling system\n"
		          . '                        '
		          . 'functions like bind(), recv()), ...'
		    },
      'childs' => { 'template' => 'childs=i',
		    'description' =>  '--childs <num>          '
			. 'Set number of preforked childs, implies mode=single.' },
      'chroot' => { 'template' => 'chroot=s',
		    'description' =>  '--chroot <dir>          '
			. 'Change rootdir to given after binding to port.' },
      'configfile' => { 'template' => 'configfile=s',
			'description' =>  '--configfile <file>     '
			    . 'Read options from config file <file>.' },
      'debug' => { 'template' => 'debug',
		   'description' =>  '--debug                 '
		       . 'Turn debugging mode on'},
      'facility' => { 'template' => 'facility=s',
		      'description' => '--facility <facility>   '
			  . 'Syslog facility; defaults to \'daemon\'' },
      'group' => { 'template' => 'group=s',
		   'description' => '--group <gid>           '
		       . 'Change gid to given group after binding to port.' },
      'help' => { 'template' => 'help',
		  'description' => '--help                  '
		      . 'Print this help message' },
      'localaddr' => { 'template' => 'localaddr=s',
		       'description' => '--localaddr <ip>        '
			   . 'IP number to bind to; defaults to INADDR_ANY' },
      'localpath' => { 'template' => 'localpath=s',
		       'description' => '--localpath <path>      '
		           . 'UNIX socket domain path to bind to' },
      'localport' => { 'template' => 'localport=s',
		       'description' => '--localport <port>      '
			   . 'Port number to bind to' },
      'logfile' => { 'template' => 'logfile=s',
		     'description' => '--logfile <file>        '
		         . 'Force logging to <file>' },
      'loop-child' => { 'template' => 'loop-child',
		       'description' => '--loop-child            '
			      . 'Create a child process for loops' },
      'loop-timeout' => { 'template' => 'loop-timeout=f',
		       'description' => '--loop-timeout <secs>   '
			      . 'Looping mode, <secs> seconds per loop' },
      'mode' => { 'template' => 'mode=s',
		  'description' => '--mode <mode>           '
		      . 'Operation mode (threads, fork or single)' },
      'pidfile' => { 'template' => 'pidfile=s',
		     'description' => '--pidfile <file>        '
			 . 'Use <file> as PID file' },
      'proto' => { 'template' => 'proto=s',
		   'description' => '--proto <protocol>        '
		   . 'transport layer protocol: tcp (default) or unix' },
      'user' => { 'template' => 'user=s',
		  'description' => '--user <user>           '
		      . 'Change uid to given user after binding to port.' },
      'version' => { 'template' => 'version',
		     'description' => '--version               '
			 . 'Print version number and exit' } }
}


############################################################################
#
#   Name:    Version (Class method)
#
#   Purpose: Returns version string
#
#   Inputs:  $class - This class
#
#   Result:  Version string; suitable for printed by "--version"
#
############################################################################

sub Version ($) {
    "Net::Daemon server, Copyright (C) 1998, Jochen Wiedmann";
}


############################################################################
#
#   Name:    Usage (Class method)
#
#   Purpose: Prints usage message
#
#   Inputs:  $class - This class
#
#   Result:  Nothing; aborts with error status
#
############################################################################

sub Usage ($) {
    my($class) = shift;
    my($options) = $class->Options();
    my(@options) = sort (keys %$options);

    print STDERR "Usage: $0 <options>\n\nPossible options are:\n\n";
    my($key);
    foreach $key (sort (keys %$options)) {
	my($o) = $options->{$key};
	print STDERR "  ", $o->{'description'}, "\n" if $o->{'description'};
    }
    print STDERR "\n", $class->Version(), "\n";
    exit(1);
}



############################################################################
#
#   Name:    ReadConfigFile (Instance method)
#
#   Purpose: Reads the config file.
#
#   Inputs:  $self - Instance
#            $file - config file name
#            $options - Hash of command line options; these are not
#                really for being processed by this method. We pass
#                it just in case. The new() method will process them
#                at a later time.
#            $args - Array ref of other command line options.
#
############################################################################

sub ReadConfigFile {
    my($self, $file, $options, $args) = @_;
    if (! -f $file) {
	$self->Fatal("No such config file: $file");
    }
    my $copts = do $file;
    if ($@) {
	$self->Fatal("Error while processing config file $file: $@");
    }
    if (!$copts || ref($copts) ne 'HASH') {
	$self->Fatal("Config file $file did not return a hash ref.");
    }
    # Override current configuration with config file options.
    while (my($var, $val) = each %$copts) {
	$self->{$var} = $val;
    }
}


############################################################################
#
#   Name:    new (Class method)
#
#   Purpose: Constructor
#
#   Inputs:  $class - This class
#            $attr - Hash ref of attributes
#            $args - Array ref of command line arguments
#
#   Result:  Server object for success, error message otherwise
#
############################################################################

sub new ($$;$) {
    my($class, $attr, $args) = @_;
    my($self) = $attr ? \%$attr : {};
    bless($self, (ref($class) || $class));

    my $options = ($self->{'options'} ||= {});
    $self->{'args'} ||= [];
    if ($args) {
	my @optList = map { $_->{'template'} } values(%{$class->Options()});

	local @ARGV = @$args;
	if (!Getopt::Long::GetOptions($options, @optList)) {
	    $self->Usage();
	}
	@{$self->{'args'}} = @ARGV;

	if ($options->{'help'}) {
	    $self->Usage();
	}
	if ($options->{'version'}) {
	    print STDERR $self->Version(), "\n";
	    exit 1;
	}
    }

    my $file = $options->{'configfile'}  ||  $self->{'configfile'};
    if ($file) {
	$self->ReadConfigFile($file, $options, $args);
    }
    while (my($var, $val) = each %$options) {
	$self->{$var} = $val;
    }

    if ($self->{'childs'}) {
	$self->{'mode'} = 'single';
    } elsif (!defined($self->{'mode'})) {
	if (eval { require thread }) {
	    $self->{'mode'} = 'ithreads';
	} elsif (eval { require Thread }) {
	    $self->{'mode'} = 'threads';
	} else {
	    my $fork = 0;
	    if ($^O ne "MSWin32") {
		my $pid = eval { fork() };
		if (defined($pid)) {
		    if (!$pid) { exit; } # Child
		    $fork = 1;
		    wait;
	        }
	    }
	    if ($fork) {
		$self->{'mode'} = 'fork';
	    } else {
		$self->{'mode'} = 'single';
	    }
	}
    }

    if ($self->{'mode'} eq 'ithreads') {
	require threads;
    } elsif ($self->{'mode'} eq 'threads') {
	require Thread;
    } elsif ($self->{'mode'} eq 'fork') {
	# Initialize forking mode ...
    } elsif ($self->{'mode'} eq 'single') {
	# Initialize single mode ...
    } else {
	$self->Fatal("Unknown operation mode: $self->{'mode'}");
    }
    $self->{'catchint'} = 1 unless exists($self->{'catchint'});
    $self->Debug("Server starting in operation mode $self->{'mode'}");
    if ($self->{'childs'}) {
	$self->Debug("Preforking $self->{'childs'} child processes ...");
    }

    $self;
}

sub Clone ($$) {
    my($proto, $client) = @_;
    my $self = { %$proto };
    $self->{'socket'} = $client;
    $self->{'parent'} = $proto;
    bless($self, ref($proto));
    $self;
}


############################################################################
#
#   Name:    Accept (Instance method)
#
#   Purpose: Called for authentication purposes
#
#   Inputs:  $self - Server instance
#
#   Result:  TRUE, if the client has successfully authorized, FALSE
#            otherwise.
#
############################################################################

sub Accept ($) {
    my $self = shift;
    my $socket = $self->{'socket'};
    my $clients = $self->{'clients'};
    my $from = $self->{'proto'} eq 'unix' ?
	"Unix socket" : sprintf("%s, port %s",
				$socket->peerhost(), $socket->peerport());

    # Host based authorization
    if ($self->{'clients'}) {
	my ($name, $aliases, $addrtype, $length, @addrs);
	if ($self->{'proto'} eq 'unix') {
	    ($name, $aliases, $addrtype, $length, @addrs) =
		('localhost', '', Socket::AF_INET(),
		 length(Socket::IN_ADDR_ANY()),
			Socket::inet_aton('127.0.0.1'));
	} else {
	    ($name, $aliases, $addrtype, $length, @addrs) =
		gethostbyaddr($socket->peeraddr(), Socket::AF_INET());
	}
	my @patterns = @addrs ?
	    map { Socket::inet_ntoa($_) } @addrs  :
	    $socket->peerhost();
	push(@patterns, $name)			if ($name);
	push(@patterns, split(/ /, $aliases))	if $aliases;

	my $found;
	OUTER: foreach my $client (@$clients) {
	    if (!$client->{'mask'}) {
		$found = $client;
		last;
	    }
	    my $masks = ref($client->{'mask'}) ?
		$client->{'mask'} : [ $client->{'mask'} ];

	    #
	    # Regular expressions aren't thread safe, as of
	    # 5.00502 :-(
	    #
	    my $lock;
	    $lock = lock($Net::Daemon::RegExpLock)
		if ($self->{'mode'} eq 'threads');
	    foreach my $mask (@$masks) {
		foreach my $alias (@patterns) {
		    if ($alias =~ /$mask/) {
			$found = $client;
			last OUTER;
		    }
		}
	    }
	}

	if (!$found  ||  !$found->{'accept'}) {
	    $self->Error("Access not permitted from $from");
	    return 0;
	}
	$self->{'client'} = $found;
    }

    $self->Debug("Accepting client from $from");
    1;
}


############################################################################
#
#   Name:    Run (Instance method)
#
#   Purpose: Does the real work
#
#   Inputs:  $self - Server instance
#
#   Result:  Nothing; returning will make the connection to be closed
#
############################################################################

sub Run ($) {
}


############################################################################
#
#   Name:    Done (Instance method)
#
#   Purpose: Called by the server before doing an accept(); a TRUE
#            value makes the server terminate.
#
#   Inputs:  $self - Server instance
#
#   Result:  TRUE or FALSE
#
#   Bugs:    Doesn't work in forking mode.
#
############################################################################

sub Done ($;$) {
    my $self = shift;
    $self->{'done'} = shift if @_;
    $self->{'done'}
}


############################################################################
#
#   Name:    Loop (Instance method)
#
#   Purpose: If $self->{'loop-timeout'} option is set, then this method
#            will be called every "loop-timeout" seconds.
#
#   Inputs:  $self - Server instance
#
#   Result:  Nothing; aborts in case of trouble. Note, that this is *not*
#            trapped and forces the server to exit.
#
############################################################################

sub Loop {
}


############################################################################
#
#   Name:    ChildFunc (Instance method)
#
#   Purpose: If possible, spawn a child process which calls a given
#	     method. In server mode single the method is called
#            directly.
#
#   Inputs:  $self - Instance
#            $method - Method name
#            @args - Method arguments
#
#   Returns: Nothing; aborts in case of problems.
#
############################################################################

sub ChildFunc {
    my($self, $method, @args) = @_;
    if ($self->{'mode'} eq 'single') {
	$self->$method(@args);
    } elsif ($self->{'mode'} eq 'threads') {
	my $startfunc = sub {
	    my $self = shift;
	    my $method = shift;
	    $self->$method(@_)
	};
	Thread->new($startfunc, $self, $method, @args)
	    or die "Failed to create a new thread: $!";
    } elsif ($self->{'mode'} eq 'ithreads') {
	my $startfunc = sub {
	    my $self = shift;
	    my $method = shift;
	    $self->$method(@_)
	};
	threads->new($startfunc, $self, $method, @args)
	    or die "Failed to create a new thread: $!";
    } else {
	my $pid = fork();
	die "Cannot fork: $!" unless defined $pid;
	return if $pid;        # Parent
	$self->$method(@args); # Child
	exit(0);
    }
}


############################################################################
#
#   Name:    Bind (Instance method)
#
#   Purpose: Binds to a port; if successfull, it never returns. Instead
#            it accepts connections. For any connection a new thread is
#            created and the Accept method is executed.
#
#   Inputs:  $self - Server instance
#
#   Result:  Error message in case of failure
#
############################################################################

sub HandleChild {
    my $self = shift;
    $self->Debug("New child starting ($self).");
    eval {
	if (!$self->Accept()) {
	    $self->Error('Refusing client');
	} else {
	    $self->Debug('Accepting client');
	    $self->Run();
	}
    };
    $self->Error("Child died: $@") if $@;
    $self->Debug("Child terminating.");
    $self->Close();
};

sub SigChildHandler {
    my $self = shift; my $ref = shift;
    return 'IGNORE' if $self->{'mode'} eq 'fork' || $self->{'childs'};
    return undef; # Don't care for childs.
}

sub Bind ($) {
    my $self = shift;
    my $fh;
    my $child_pid;

    my $reaper = $self->SigChildHandler(\$child_pid);
    $SIG{'CHLD'} = $reaper if $reaper;

    if (!$self->{'socket'}) {
	$self->{'proto'} ||= ($self->{'localpath'}) ? 'unix' : 'tcp';

	if ($self->{'proto'} eq 'unix') {
	    my $path = $self->{'localpath'}
		or $self->Fatal('Missing option: localpath');
	    unlink $path;
	    $self->Fatal("Can't remove stale Unix socket ($path): $!")
		if -e $path;
	    my $old_umask = umask 0;
	    $self->{'socket'} =
		IO::Socket::UNIX->new('Local' => $path,
				      'Listen' => $self->{'listen'} || 10)
		      or $self->Fatal("Cannot create Unix socket $path: $!");
	    umask $old_umask;
	} else {
	    $self->{'socket'} = IO::Socket::INET->new
		( 'LocalAddr' => $self->{'localaddr'},
		  'LocalPort' => $self->{'localport'},
		  'Proto'     => $self->{'proto'} || 'tcp',
		  'Listen'    => $self->{'listen'} || 10,
		  'Reuse'     => 1)
		    or $self->Fatal("Cannot create socket: $!");
	}
    }
    $self->Log('notice', "Server starting");

    if ((my $pidfile = ($self->{'pidfile'} || '')) ne 'none') {
	$self->Debug("Writing PID to $pidfile");
	my $fh = Symbol::gensym();
	$self->Fatal("Cannot write to $pidfile: $!")
	    unless (open (OUT, ">$pidfile")
		    and (print OUT "$$\n")
		    and close(OUT));
    }

    if (my $dir = $self->{'chroot'}) {
	$self->Debug("Changing root directory to $dir");
	if (!chroot($dir)) {
	    $self->Fatal("Cannot change root directory to $dir: $!");
	}
    }
    if (my $group = $self->{'group'}) {
	$self->Debug("Changing GID to $group");
	my $gid;
	if ($group !~ /^\d+$/) {
	    if (defined(my $gid = getgrnam($group))) {
		$group = $gid;
	    } else {
		$self->Fatal("Cannot determine gid of $group: $!");
	    }
	}
	$( = ($) = $group);
    }
    if (my $user = $self->{'user'}) {
	$self->Debug("Changing UID to $user");
	my $uid;
	if ($user !~ /^\d+$/) {
	    if (defined(my $uid = getpwnam($user))) {
		$user = $uid;
	    } else {
		$self->Fatal("Cannot determine uid of $user: $!");
	    }
	}
	$< = ($> = $user);
    }

    if ($self->{'childs'}) {
      my $pid;

      my $childpids = $self->{'childpids'} = {};
      for (my $n = 0; $n < $self->{'childs'}; $n++) {
	$pid = fork();
	die "Cannot fork: $!" unless defined $pid;
	if (!$pid) { #Child
	  $self->{'mode'} = 'single';
	  last;
	}
	# Parent
	$childpids->{$pid} = 1;
      }
      if ($pid) {
	# Parent waits for childs in a loop, then exits ...
	# We could also terminate the parent process, but
	# if the parent is still running we can kill the
	# whole group by killing the childs.
	my $childpid;
	$exit = 0;
	$SIG{'TERM'} = sub { die };
	$SIG{'INT'}  = sub { die };
	eval {
	  do {
	    $childpid = wait;
	    delete $childpids->{$childpid};
	    $self->Debug("Child $childpid has exited");
	  } until ($childpid <= 0 || $exit || keys(%$childpids) == 0);
	};
	my @pids = keys %{$self -> {'childpids'}};
	if (@pids) {
	  $self->Debug("kill TERM childs: " . join(",", @pids));
	  kill 'TERM', @pids if @pids ; # send a TERM to all childs
	}
	exit (0);
      }
    }

    my $time = $self->{'loop-timeout'} ?
	(time() + $self->{'loop-timeout'}) : 0;

    my $client;
    while (!$self->Done()) {
	undef $child_pid;
	my $rin = '';
	vec($rin,$self->{'socket'}->fileno(),1) = 1;
	my($rout, $t);
	if ($time) {
	    my $tm = time();
	    $t = $time - $tm;
	    $t = 0 if $t < 0;
	    $self->Debug("Loop time: time=$time now=$tm, t=$t");
	}
	my($nfound) = select($rout=$rin, undef, undef, $t);
	if ($nfound < 0) {
	    if (!$child_pid  and
		($! != POSIX::EINTR() or !$self->{'catchint'})) {
		$self->Fatal("%s server failed to select(): %s",
			     ref($self), $self->{'socket'}->error() || $!);
	    }
	} elsif ($nfound) {
	    my $client = $self->{'socket'}->accept();
	    if (!$client) {
		if (!$child_pid  and
		    ($! != POSIX::EINTR() or !$self->{'catchint'})) {
		    $self->Error("%s server failed to accept: %s",
				 ref($self), $self->{'socket'}->error() || $!);
		}
	    } else {
		if ($self->{'debug'}) {
		    my $from = $self->{'proto'} eq 'unix' ?
			'Unix socket' :
			sprintf('%s, port %s',
				# SE 19990917: display client data!!
				$client->peerhost(),
				$client->peerport());
		    $self->Debug("Connection from $from");
		}
		my $sth = $self->Clone($client);
		$self->Debug("Child clone: $sth\n");
		$sth->ChildFunc('HandleChild') if $sth;
		if ($self->{'mode'} eq 'fork') {
		    close($client);
		}
	    }
	}
	if ($time) {
	    my $t = time();
	    if ($t >= $time) {
		$time = $t;
		if ($self->{'loop-child'}) {
		    $self->ChildFunc('Loop');
		} else {
		    $self->Loop();
		}
		$time += $self->{'loop-timeout'};
	    }
	}
    }
    $self->Log('notice', "%s server terminating", ref($self));
}

sub Close {
    my $socket = shift->{'socket'};
    $socket->close() if $socket;
}


1;

__END__

=head1 NAME

Net::Daemon - Perl extension for portable daemons


=head1 SYNOPSIS

  # Create a subclass of Net::Daemon
  require Net::Daemon;
  package MyDaemon;
  @MyDaemon::ISA = qw(Net::Daemon);

  sub Run ($) {
    # This function does the real work; it is invoked whenever a
    # new connection is made.
  }


=head1 DESCRIPTION

Net::Daemon is an abstract base class for implementing portable server
applications in a very simple way. The module is designed for Perl 5.005
and threads, but can work with fork() and Perl 5.004.

The Net::Daemon class offers methods for the most common tasks a daemon
needs: Starting up, logging, accepting clients, authorization, restricting
its own environment for security and doing the true work. You only have to
override those methods that aren't appropriate for you, but typically
inheriting will safe you a lot of work anyways.


=head2 Constructors

  $server = Net::Daemon->new($attr, $options);

  $connection = $server->Clone($socket);

Two constructors are available: The B<new> method is called upon startup
and creates an object that will basically act as an anchor over the
complete program. It supports command line parsing via L<Getopt::Long (3)>.

Arguments of B<new> are I<$attr>, an hash ref of attributes (see below)
and I<$options> an array ref of options, typically command line arguments
(for example B<\@ARGV>) that will be passed to B<Getopt::Long::GetOptions>.

The second constructor is B<Clone>: It is called whenever a client
connects. It receives the main server object as input and returns a
new object. This new object will be passed to the methods that finally
do the true work of communicating with the client. Communication occurs
over the socket B<$socket>, B<Clone>'s argument.

Possible object attributes and the corresponding command line
arguments are:

=over 4

=item I<catchint> (B<--nocatchint>)

On some systems, in particular Solaris, the functions accept(),
read() and so on are not safe against interrupts by signals. For
example, if the user raises a USR1 signal (as typically used to
reread config files), then the function returns an error EINTR.
If the I<catchint> option is on (by default it is, use
B<--nocatchint> to turn this off), then the package will ignore
EINTR errors whereever possible.

=item I<chroot> (B<--chroot=dir>)

(UNIX only)  After doing a bind(), change root directory to the given
directory by doing a chroot(). This is usefull for security operations,
but it restricts programming a lot. For example, you typically have to
load external Perl extensions before doing a chroot(), or you need to
create hard links to Unix sockets. This is typically done in the config
file, see the --configfile option. See also the --group and --user
options.

If you don't know chroot(), think of an FTP server where you can see a
certain directory tree only after logging in.

=item I<clients>

An array ref with a list of clients. Clients are hash refs, the attributes
I<accept> (0 for denying access and 1 for permitting) and I<mask>, a Perl
regular expression for the clients IP number or its host name. See
L<"Access control"> below.

=item I<configfile> (B<--configfile=file>)

Net::Daemon supports the use of config files. These files are assumed
to contain a single hash ref that overrides the arguments of the new
method. However, command line arguments in turn take precedence over
the config file. See the L<"Config File"> section below for details
on the config file.

=item I<debug> (B<--debug>)

Turn debugging mode on. Mainly this asserts that logging messages of
level "debug" are created.

=item I<facility> (B<--facility=mode>)

(UNIX only) Facility to use for L<Sys::Syslog (3)>. The default is
B<daemon>.

=item I<group> (B<--group=gid>)

After doing a bind(), change the real and effective GID to the given.
This is usefull, if you want your server to bind to a privileged port
(<1024), but don't want the server to execute as root. See also
the --user option.

GID's can be passed as group names or numeric values.

=item I<localaddr> (B<--localaddr=ip>)

By default a daemon is listening to any IP number that a machine
has. This attribute allows to restrict the server to the given
IP number.

=item I<localpath> (B<--localpath=path>)

If you want to restrict your server to local services only, you'll
prefer using Unix sockets, if available. In that case you can use
this option for setting the path of the Unix socket being created.
This option implies B<--proto=unix>.

=item I<localport> (B<--localport=port>)

This attribute sets the port on which the daemon is listening. It
must be given somehow, as there's no default.

=item I<logfile> (B<--logfile=file>)

By default logging messages will be written to the syslog (Unix) or
to the event log (Windows NT). On other operating systems you need to
specify a log file. The special value "STDERR" forces logging to
stderr.

=item I<loop-child> (B<--loop-child>)

This option forces creation of a new child for loops. (See the
I<loop-timeout> option.) By default the loops are serialized.

=item I<loop-timeout> (B<--loop-timeout=secs>)

Some servers need to take an action from time to time. For example the
Net::Daemon::Spooler attempts to empty its spooling queue every 5
minutes. If this option is set to a positive value (zero being the
default), then the server will call its Loop method every "loop-timeout"
seconds.

Don't trust too much on the precision of the interval: It depends on
a number of factors, in particular the execution time of the Loop()
method. The loop is implemented by using the I<select> function. If
you need an exact interval, you should better try to use the alarm()
function and a signal handler. (And don't forget to look at the
I<catchint> option!)

It is recommended to use the I<loop-child> option in conjunction with
I<loop-timeout>.

=item I<mode> (B<--mode=modename>)

The Net::Daemon server can run in three different modes, depending on
the environment.

If you are running Perl 5.005 and did compile it for threads, then
the server will create a new thread for each connection. The thread
will execute the server's Run() method and then terminate. This mode
is the default, you can force it with "--mode=ithreads" or
"--mode=threads".

If threads are not available, but you have a working fork(), then the
server will behave similar by creating a new process for each connection.
This mode will be used automatically in the absence of threads or if
you use the "--mode=fork" option.

Finally there's a single-connection mode: If the server has accepted a
connection, he will enter the Run() method. No other connections are
accepted until the Run() method returns. This operation mode is useful
if you have neither threads nor fork(), for example on the Macintosh.
For debugging purposes you can force this mode with "--mode=single".

When running in mode single, you can still handle multiple clients at
a time by preforking multiple child processes. The number of childs
is configured with the option "--childs".

=item I<childs>

Use this parameter to let Net::Daemon run in prefork mode, which means
it forks the number of childs processes you give with this parameter,
and all child handle connections concurrently. The difference to
fork mode is, that the child processes continue to run after a
connection has terminated and are able to accept a new connection.
This is useful for caching inside the childs process (e.g.
DBI::ProxyServer connect_cached attribute)

=item I<options>

Array ref of Command line options that have been passed to the server object
via the B<new> method.

=item I<parent>

When creating an object with B<Clone> the original object becomes
the parent of the new object. Objects created with B<new> usually
don't have a parent, thus this attribute is not set.

=item I<pidfile> (B<--pidfile=file>)

(UNIX only) If this option is present, a PID file will be created at the
given location.

=item I<proto> (B<--proto=proto>)

The transport layer to use, by default I<tcp> or I<unix> for a Unix
socket. It is not yet possible to combine both.

=item I<socket>

The socket that is connected to the client; passed as B<$client> argument
to the B<Clone> method. If the server object was created with B<new>,
this attribute can be undef, as long as the B<Bind> method isn't called.
Sockets are assumed to be IO::Socket objects.

=item I<user> (B<--user=uid>)

After doing a bind(), change the real and effective UID to the given.
This is usefull, if you want your server to bind to a privileged port
(<1024), but don't want the server to execute as root. See also
the --group and the --chroot options.

UID's can be passed as group names or numeric values.

=item I<version> (B<--version>)

Supresses startup of the server; instead the version string will
be printed and the program exits immediately.

=back

Note that most of these attributes (facility, mode, localaddr, localport,
pidfile, version) are meaningfull only at startup. If you set them later,
they will be simply ignored. As almost all attributes have appropriate
defaults, you will typically use the B<localport> attribute only.


=head2 Command Line Parsing

  my $optionsAvailable = Net::Daemon->Options();

  print Net::Daemon->Version(), "\n";

  Net::Daemon->Usage();

The B<Options> method returns a hash ref of possible command line options.
The keys are option names, the values are again hash refs with the
following keys:

=over 4

=item template

An option template that can be passed to B<Getopt::Long::GetOptions>.

=item description

A description of this option, as used in B<Usage>

=back

The B<Usage> method prints a list of all possible options and returns.
It uses the B<Version> method for printing program name and version.


=head2 Config File

If the config file option is set in the command line options or in the
in the "new" args, then the method

  $server->ReadConfigFile($file, $options, $args)

is invoked. By default the config file is expected to contain Perl source
that returns a hash ref of options. These options override the "new"
args and will in turn be overwritten by the command line options, as
present in the $options hash ref.

A typical config file might look as follows, we use the DBI::ProxyServer
as an example:

    # Load external modules; this is not required unless you use
    # the chroot() option.
    #require DBD::mysql;
    #require DBD::CSV;

    {
	# 'chroot' => '/var/dbiproxy',
	'facility' => 'daemon',
	'pidfile' => '/var/dbiproxy/dbiproxy.pid',
	'user' => 'nobody',
	'group' => 'nobody',
	'localport' => '1003',
	'mode' => 'fork'

	# Access control
        'clients' => [
	    # Accept the local
	    {
		'mask' => '^192\.168\.1\.\d+$',
                'accept' => 1
            },
	    # Accept myhost.company.com
	    {
		'mask' => '^myhost\.company\.com$',
                'accept' => 1
            }
	    # Deny everything else
	    {
		'mask' => '.*',
		'accept' => 0
	    }
        ]
    }


=head2 Access control

The Net::Daemon package supports a host based access control scheme. By
default access is open for anyone. However, if you create an attribute
$self->{'clients'}, typically in the config file, then access control
is disabled by default. For any connection the client list is processed:
The clients attribute is an array ref to a list of hash refs. Any of the
hash refs may contain arbitrary attributes, including the following:

=over 8

=item mask

A Perl regular expression that has to match the clients IP number or
its host name. The list is processed from the left to the right, whenever
a 'mask' attribute matches, then the related hash ref is choosen as
client and processing the client list stops.

=item accept

This may be set to true or false (default when omitting the attribute),
the former means accepting the client.

=back


=head2 Event logging

  $server->Log($level, $format, @args);
  $server->Debug($format, @args);
  $server->Error($format, @args);
  $server->Fatal($format, @args);

The B<Log> method is an interface to L<Sys::Syslog (3)> or
L<Win32::EventLog (3)>. It's arguments are I<$level>, a syslog
level like B<debug>, B<notice> or B<err>, a format string in the
style of printf and the format strings arguments.

The B<Debug> and B<Error> methods are shorthands for calling
B<Log> with a level of debug and err, respectively. The B<Fatal>
method is like B<Error>, except it additionally throws the given
message as exception.

See L<Net::Daemon::Log(3)> for details.


=head2 Flow of control

  $server->Bind();
  # The following inside Bind():
  if ($connection->Accept()) {
      $connection->Run();
  } else {
      $connection->Log('err', 'Connection refused');
  }

The B<Bind> method is called by the application when the server should
start. Typically this can be done right after creating the server object
B<$server>. B<Bind> usually never returns, except in case of errors.

When a client connects, the server uses B<Clone> to derive a connection
object B<$connection> from the server object. A new thread or process
is created that uses the connection object to call your classes
B<Accept> method. This method is intended for host authorization and
should return either FALSE (refuse the client) or TRUE (accept the client).

If the client is accepted, the B<Run> method is called which does the
true work. The connection is closed when B<Run> returns and the corresponding
thread or process exits.


=head2 Error Handling

All methods are supposed to throw Perl exceptions in case of errors.


=head1 MULTITHREADING CONSIDERATIONS

All methods are working with lexically scoped data and handle data
only, the exception being the OpenLog method which is invoked before
threading starts. Thus you are safe as long as you don't share
handles between threads. I strongly recommend that your application
behaves similar. (This doesn't apply to mode 'ithreads'.)



=head1 EXAMPLE

As an example we'll write a simple calculator server. After connecting
to this server you may type expressions, one per line. The server
evaluates the expressions and prints the result. (Note this is an example,
in real life we'd never implement such a security hole. :-)

For the purpose of example we add a command line option I<--base> that
takes 'hex', 'oct' or 'dec' as values: The servers output will use the
given base.

  # -*- perl -*-
  #
  # Calculator server
  #
  require 5.004;
  use strict;

  require Net::Daemon;


  package Calculator;

  use vars qw($VERSION @ISA);
  $VERSION = '0.01';
  @ISA = qw(Net::Daemon); # to inherit from Net::Daemon

  sub Version ($) { 'Calculator Example Server, 0.01'; }

  # Add a command line option "--base"
  sub Options ($) {
      my($self) = @_;
      my($options) = $self->SUPER::Options();
      $options->{'base'} = { 'template' => 'base=s',
			     'description' => '--base                  '
				    . 'dec (default), hex or oct'
			      };
      $options;
  }

  # Treat command line option in the constructor
  sub new ($$;$) {
      my($class, $attr, $args) = @_;
      my($self) = $class->SUPER::new($attr, $args);
      if ($self->{'parent'}) {
	  # Called via Clone()
	  $self->{'base'} = $self->{'parent'}->{'base'};
      } else {
	  # Initial call
	  if ($self->{'options'}  &&  $self->{'options'}->{'base'}) {
	      $self->{'base'} = $self->{'options'}->{'base'}
          }
      }
      if (!$self->{'base'}) {
	  $self->{'base'} = 'dec';
      }
      $self;
  }

  sub Run ($) {
      my($self) = @_;
      my($line, $sock);
      $sock = $self->{'socket'};
      while (1) {
	  if (!defined($line = $sock->getline())) {
	      if ($sock->error()) {
		  $self->Error("Client connection error %s",
			       $sock->error());
	      }
	      $sock->close();
	      return;
	  }
	  $line =~ s/\s+$//; # Remove CRLF
	  my($result) = eval $line;
	  my($rc);
	  if ($self->{'base'} eq 'hex') {
	      $rc = printf $sock ("%x\n", $result);
	  } elsif ($self->{'base'} eq 'oct') {
	      $rc = printf $sock ("%o\n", $result);
	  } else {
	      $rc = printf $sock ("%d\n", $result);
	  }
	  if (!$rc) {
	      $self->Error("Client connection error %s",
			   $sock->error());
	      $sock->close();
	      return;
	  }
      }
  }

  package main;

  my $server = Calculator->new({'pidfile' => 'none',
				'localport' => 2000}, \@ARGV);
  $server->Bind();


=head1 KNOWN PROBLEMS

Most, or even any, known problems are related to the Sys::Syslog module
which is by default used for logging events under Unix. I'll quote some
examples:

=over

=item Usage: Sys::Syslog::_PATH_LOG at ...

This problem is treated in perl bug 20000712.003. A workaround is
changing line 277 of Syslog.pm to

  my $syslog = &_PATH_LOG() || croak "_PATH_LOG not found in syslog.ph";

=back


=head1 AUTHOR AND COPYRIGHT

  Net::Daemon is Copyright (C) 1998, Jochen Wiedmann
                                     Am Eisteich 9
                                     72555 Metzingen
                                     Germany

                                     Phone: +49 7123 14887
                                     Email: joe@ispsoft.de

  All rights reserved.

  You may distribute this package under the terms of either the GNU
  General Public License or the Artistic License, as specified in the
  Perl README file.


=head1 SEE ALSO

L<RPC::pServer(3)>, L<Netserver::Generic(3)>, L<Net::Daemon::Log(3)>,
L<Net::Daemon::Test(3)>

=cut

