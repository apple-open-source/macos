# -*- perl -*-
#
#   $Id: Test.pm,v 1.2 1999/08/12 14:28:57 joe Exp $
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
#
#   This module is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This module is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this module; if not, write to the Free Software
#   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
############################################################################

package Net::Daemon::Test;

use strict;
require 5.004;

use Net::Daemon ();
use Symbol ();
use File::Basename ();


$Net::Daemon::Test::VERSION = '0.03';
@Net::Daemon::Test::ISA = qw(Net::Daemon);


=head1 NAME

Net::Daemon::Test - support functions for testing Net::Daemon servers


=head1 SYNOPSIS

    # This is the server, stored in the file "servertask".
    #
    # Create a subclass of Net::Daemon::Test, which in turn is
    # a subclass of Net::Daemon
    use Net::Daemon::Test ();
    package MyDaemon;
    @MyDaemon::ISA = qw(Net::Daemon::Test);

    sub Run {
	# Overwrite this and other methods, as you like.
    }

    my $self = Net::Daemon->new(\%attr, \@options);
    eval { $self->Bind() };
    if ($@) {
	die "Server cannot bind: $!";
    }
    eval { $self->Run() };
    if ($@) {
	die "Unexpected server termination: $@";
    }


    # This is the client, the real test script, note we call the
    # "servertask" file below:
    #
    # Call the Child method to spawn a child. Don't forget to use
    # the timeout option.
    use Net::Daemon::Test ();

    my($handle, $port) = eval {
        Net::Daemon::Test->Child(5, # Number of subtests
				 'servertask', '--timeout', '20')
    };
    if ($@) {
	print "not ok 1 $@\n";
	exit 0;
    }
    print "ok 1\n";

    # Real tests following here
    ...

    # Terminate the server
    $handle->Terminate();


=head1 DESCRIPTION

This module is a frame for creating test scripts of Net::Daemon based
server packages, preferrably using Test::Harness, but that's your
choice.

A test consists of two parts: The client part and the server part.
The test is executed by the child part which invokes the server part,
by spawning a child process and invoking an external Perl script.
(Of course we woultn't need this external file with fork(), but that's
the best possibility to make the test scripts portable to Windows
without requiring threads in the test script.)

The server part is a usual Net::Daemon application, for example a script
like dbiproxy. The only difference is that it derives from
Net::Daemon::Test and not from Net::Daemon, the main difference is that
the B<Bind> method attempts to allocate a port automatically. Once a
port is allocated, the number is stored in the file "ndtest.prt".

After spawning the server process, the child will wait ten seconds
(hopefully sufficient) for the creation of ndtest.prt.


=head1 AVAILABLE METHODS

=head2 Server part

=over 8

=item Options

Adds an option B<--timeout> to Net::Daemon: The server's Run method
will die after at most 20 seconds.

=cut

sub Options ($) {
    my $self = shift;
    my $options = $self->SUPER::Options();
    $options->{'timeout'} = {
	'template' => 'timeout=i',
	'description' => '--timeout <secs>        '
	    . "The server will die if the test takes longer\n"
	    . '                        than this number of seconds.'
	};
    $options;
}


=pod

=item Bind

(Instance method) This is mainly the default Bind method, but it attempts
to find and allocate a free port in two ways: First of all, it tries to
call Bind with port 0, most systems will automatically choose a port in
that case. If that seems to fail, ports 30000-30049 are tried. We
hope, one of these will succeed. :-)

=cut

sub Bind ($) {
    # First try: Pass unmodified options to Net::Daemon::Bind
    my $self = shift;
    my($port, $socket);
    $self->{'proto'} ||= $self->{'localpath'} ? 'unix' : 'tcp';
    if ($self->{'proto'} eq 'unix') {
        $port = $self->{'localpath'} || die "Missing option: localpath";
        $socket = eval {
            IO::Socket::UNIX->new('Local' => $port,
                                  'Listen' => $self->{'listen'} || 10);
        }
    } else {
        my @socket_args =
	    ( 'LocalAddr' => $self->{'localaddr'},
	      'LocalPort' => $self->{'localport'},
	      'Proto' => $self->{'proto'} || 'tcp',
	      'Listen' => $self->{'listen'} || 10,
	      'Reuse' => 1
	    );
        $socket = eval { IO::Socket::INET->new(@socket_args) };
        if ($socket) {
	    $port = $socket->sockport();
        } else {
            $port = 30049;
            while (!$socket  &&  $port++ < 30060) {
	        $socket = eval { IO::Socket::INET->new(@socket_args,
	       			                       'LocalPort' => $port) };
            }
        }
    }
    if (!$socket) {
	die "Cannot create socket: " . ($@ || $!);
    }

    # Create the "ndtest.prt" file so that the child knows to what
    # port it may connect.
    my $fh = Symbol::gensym();
    if (!open($fh, ">ndtest.prt")  ||
	!(print $fh $port)  ||
	!close($fh)) {
	die "Error while creating 'ndtest.prt': $!";
    }
    $self->Debug("Created ndtest.prt with port $port\n");
    $self->{'socket'} = $socket;

    if (my $timeout = $self->{'timeout'}) {
	eval { alarm $timeout };
    }

    $self->SUPER::Bind();
}


=pod

=item Run

(Instance method) Overwrites the Net::Daemon's method by adding a timeout.

=back

sub Run ($) {
    my $self = shift;
    $self->Run();
}


=head2 Client part

=over 8

=item Child

(Class method) Attempts to spawn a server process. The server process is
expected to create the file 'ndtest.prt' with the port number.

The method returns a process handle and a port number. The process handle
offers a method B<Terminate> that may later be used to stop the server
process.

=back

=cut

sub Child ($$@) {
    my $self = shift;  my $numTests = shift;
    my($handle, $pid);

    my $args = join(" ", @_);
    print "Starting server: $args\n";

    unlink 'ndtest.prt';

    if ($args =~ /\-\-mode=(?:ithread|thread|single)/  &&  $^O =~ /mswin32/i) {
	require Win32;
	require Win32::Process;
	my $proc = $_[0];

	# Win32::Process seems to require an absolute path; this includes
	# a program extension like ".exe"
	my $path;
	my @pdirs;

	File::Basename::fileparse_set_fstype("MSWin32");
	if (File::Basename::basename($proc) !~ /\./) {
	    $proc .= ".exe";
	}
	if ($proc !~ /^\w\:\\/  &&  $proc !~ /^\\/) {
	    # Doesn't look like an absolute path
	    foreach my $dir (@pdirs = split(/;/, $ENV{'PATH'})) {
		if (-x "$dir/$proc") {
		    $path = "$dir/$proc";
		    last;
		}
	    }
	    if (!$path) {
		print STDERR ("Cannot find $proc in the following"
			      , " directories:\n");
		foreach my $dir (@pdirs) {
		    print STDERR "    $dir\n";
		}
		print STDERR "Terminating.\n";
		exit 1;
	    }
	} else {
	    $path = $proc;
	}

	print "Starting process: proc = $path, args = ", join(" ", @_), "\n";
	if (!&Win32::Process::Create($pid, $path,
 				     join(" ", @_), 0,
                                     Win32::Process::DETACHED_PROCESS(),
 				     ".")) {
 	    die "Cannot create child process: "
 		. Win32::FormatMessage(Win32::GetLastError());
 	}
	$handle = bless(\$pid, "Net::Daemon::Test::Win32");
    } else {
	$pid = eval { fork() };
	if (defined($pid)) {
	    # Aaaah, Unix! :-)
	    if (!$pid) {
		# This is the child process, spawn the server.
		exec @_;
	    }
	    $handle = bless(\$pid, "Net::Daemon::Test::Fork");
	} else {
	    print "1..0\n";
	    exit 0;
	}
    }

    print "1..$numTests\n" if defined($numTests);
    for (my $secs = 20;  $secs  &&  ! -s 'ndtest.prt';  $secs -= sleep 1) {
    }
    if (! -s 'ndtest.prt') {
	die "Server process didn't create a file 'ndtest.prt'.";
    }
    # Sleep another second in case the server is still creating the
    # file with the port number ...
    sleep 1;
    my $fh = Symbol::gensym();
    my $port;
    if (!open($fh, "<ndtest.prt")  ||
	!defined($port = <$fh>)) {
	die "Error while reading 'ndtest.prt': $!";
    }
    ($handle, $port);
}


package Net::Daemon::Test::Fork;

sub Terminate ($) {
    my $self = shift;
    my $pid = $$self;
    kill 'TERM', $pid;
}

package Net::Daemon::Test::Win32;

sub Terminate ($) {
    my $self = shift;
    my $pid = $$self;
    $pid->Kill(0);
}

1;

=head1 AUTHOR AND COPYRIGHT

  Net::Daemon is Copyright (C) 1998, Jochen Wiedmann
                                     Am Eisteich 9
                                     72555 Metzingen
                                     Germany

                                     Phone: +49 7123 14887
                                     Email: joe@ispsoft.de

  All rights reserved.

You may distribute under the terms of either the GNU General Public
License or the Artistic License, as specified in the Perl README file.


=head1 SEE ALSO

L<Net::Daemon(3)>, L<Test::Harness(3)>

=cut
