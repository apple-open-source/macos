#   -*- perl -*-
#
#
#   PlRPC - Perl RPC, package for writing simple, RPC like clients and
#       servers
#
#
#   Copyright (c) 1997,1998  Jochen Wiedmann
#
#   You may distribute under the terms of either the GNU General Public
#   License or the Artistic License, as specified in the Perl README file.
#
#   Author: Jochen Wiedmann
#           Email: jochen.wiedmann at freenet.de
#
#

use strict;

require Net::Daemon;
require RPC::PlServer::Comm;


package RPC::PlServer;

@RPC::PlServer::ISA = qw(Net::Daemon);
$RPC::PlServer::VERSION = '0.2020';


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
    "RPC::PlServer application, Copyright (C) 1997, 1998, Jochen Wiedmann";
}


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
    my $options = shift->SUPER::Options();
    $options->{'maxmessage'} =
	{ 'template' => 'maxmessage=i',
	  'description' =>  '--maxmessage <size>           '
	  . 'Set max message size to <size> (Default 65535).'
	};
    $options->{'compression'} =
	{ 'template' => 'compression=s',
	  'description' =>  '--compression <type>           '
	  . 'Set compression type to off (default) or gzip.'
	};
    $options;
}


############################################################################
#
#   Name:    AcceptApplication, AcceptVersion, AcceptUser
#            (Instance methods)
#
#   Purpose: Called for authentication purposes; these three in common
#            are replacing Net::Daemon's Accept().
#
#   Inputs:  $self - Server instance
#            $app - Application name
#            $version - Version number
#            $user, $password - User name and password
#
#   Result:  TRUE, if the client has successfully authorized, FALSE
#            otherwise. The AcceptUser method (being called as the
#            last) may additionally return an array ref as a TRUE
#            value: This is treated as welcome message.
#
############################################################################

sub AcceptApplication ($$) {
    my $self = shift; my $app = shift;
    $self->Debug("Client requests application $app");
    UNIVERSAL::isa($self, $app);
}

sub AcceptVersion ($$) {
    my $self = shift; my $version = shift;
    $self->Debug("Client requests version $version");
    no strict 'refs';
    my $myversion = ${ref($self) . "::VERSION"};
    ($version <= $myversion) ? 1 : 0;
}

sub AcceptUser ($$$) {
    my $self = shift; my $user = shift; my $password = shift;

    my $client = $self->{'client'};
    return 1 unless $client->{'users'};
    my $users = $client->{'users'};
    foreach my $u (@$users) {
	my $au;
	if (ref($u)) {
	    $au = $u;
	    $u = defined($u->{'name'}) ? $u->{'name'} : '';
	}
	if ($u eq $user) {
	    $self->{'authorized_user'} = $au;
	    return 1;
	}
    }
    0;
}

sub Accept ($) {
    my $self = shift;
    my $socket = $self->{'socket'};
    my $comm = $self->{'comm'};
    return 0 if (!$self->SUPER::Accept());
    my $client;
    if ($client = $self->{'client'}) {
	if (my $cipher = $client->{'cipher'}) {
	    $self->Debug("Host encryption: %s", $cipher);
	    $self->{'cipher'} = $cipher;
	}
    }

    my $msg = $comm->Read($socket);
    die "Unexpected EOF from client" unless defined $msg;
    die "Login message: Expected array, got $msg" unless ref($msg) eq 'ARRAY';

    my $app      = $self->{'application'} = $msg->[0] || '';
    my $version  = $self->{'version'}     = $msg->[1] || 0;
    my $user     = $self->{'user'}        = $msg->[2] || '';
    my $password = $self->{'password'}    = $msg->[3] || '';

    $self->Debug("Client logs in: Application %s, version %s, user %s",
		 $app, $version, $user);

    if (!$self->AcceptApplication($app)) {
	$comm->Write($socket,
		     [0, "This is a " . ref($self) . " server, go away!"]);
	return 0;
    }
    if (!$self->AcceptVersion($version)) {
	$comm->Write($socket,
		     [0, "Sorry, but I am not running version $version."]);
	return 0;
    }
    my $result;
    if (!($result = $self->AcceptUser($user, $password))) {
	$comm->Write($socket,
		     [0, "User $user is not permitted to connect."]);
	return 0;
    }
    $comm->Write($socket, (ref($result) ? $result : [1, "Welcome!"]));
    if (my $au = $self->{'authorized_user'}) {
	if (ref($au)  &&  (my $cipher = $au->{'cipher'})) {
	    $self->Debug("User encryption: %s", $cipher);
	    $self->{'cipher'} = $cipher;
	}
    }

    if (my $client = $self->{'client'}) {
	if (my $methods = $client->{'methods'}) {
	    $self->{'methods'} = $methods;
	}
    }
    if (my $au = $self->{'authorized_user'}) {
	if (my $methods = $au->{'methods'}) {
	    $self->{'methods'} = $methods;
	}
    }

    1;
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
    my $self = shift->SUPER::new(@_);
    $self->{'comm'} = RPC::PlServer::Comm->new($self);
    $self;
}


############################################################################
#
#   Name:    Run
#
#   Purpose: Process client requests
#
#   Inputs:  $self - Server instance
#
#   Returns: Nothing, dies in case of errors.
#
############################################################################

sub Run ($) {
    my $self = shift;
    my $comm = $self->{'comm'};
    my $socket = $self->{'socket'};

    while (!$self->Done()) {
	my $msg = $comm->Read($socket);
	last unless defined($msg);
	die "Expected array" unless ref($msg) eq 'ARRAY';
	my($error, $command);
	if (!($command = shift @$msg)) {
	    $error = "Expected method name";
	} else {
	    if ($self->{'methods'}) {
		my $class = $self->{'methods'}->{ref($self)};
		if (!$class  ||  !$class->{$command}) {
		    $error = "Not permitted for method $command of class "
			. ref($self);
		}
	    }
	    if (!$error) {
		$self->Debug("Client executes method $command");
		my @result = eval { $self->$command(@$msg) };
		if ($@) {
		    $error = "Failed to execute method $command: $@";
		} else {
		    $comm->Write($socket, \@result);
		}
	    }
	}
	if ($error) {
	    $comm->Write($socket, \$error);
	}
    }
}


############################################################################
#
#   Name:    StoreHandle, NewHandle, UseHandle, DestroyHandle,
#            CallMethod
#
#   Purpose: Support functions for working with objects
#
#   Inputs:  $self - server instance
#            $object - Server side object
#            $handle - Client side handle
#
############################################################################

sub StoreHandle ($$) {
    my $self = shift; my $object = shift;
    my $handle = "$object";
    $self->{'handles'}->{$handle} = $object;
    $handle;
}

sub NewHandle ($$$@) {
    my($self, $handle, $method, @args) = @_;
    my $object = $self->CallMethod($handle, $method, @args);
    die "Constructor $method didn't return a true value" unless $object;
    $self->StoreHandle($object)
}

sub UseHandle ($$) {
    my $self = shift; my $handle = shift;
    $self->{'handles'}->{$handle}  ||  die "No such object: $handle";
}

sub DestroyHandle ($$) {
    my $self = shift; my $handle = shift;
    (delete $self->{'handles'}->{$handle})  ||  die "No such object: $handle";
    ();
}

sub CallMethod ($$$@) {
    my($self, $handle, $method, @args) = @_;
    my($ref, $object);

    my $call_by_instance;
    {
	my $lock = lock($Net::Daemon::RegExpLock)
	    if $Net::Daemon::RegExpLock && $self->{'mode'} eq 'threads';
	$call_by_instance = ($handle =~ /=\w+\(0x/);
    }
    if ($call_by_instance) {
	# Looks like a call by instance
	$object = $self->UseHandle($handle);
	$ref = ref($object);
    } else {
	# Call by class
	$ref = $object = $handle;
    }

    if ($self->{'methods'}) {
	my $class = $self->{'methods'}->{$ref};
	if (!$class  ||  !$class->{$method}) {
	    die "Not permitted for method $method of class $ref";
	}
    }

    $object->$method(@args);
}


1;


__END__

=head1 NAME

RPC::PlServer - Perl extension for writing PlRPC servers

=head1 SYNOPSIS

  # Create a subclass of RPC::PlServer
  use RPC::PlServer;

  package MyServer;
  $MyServer::VERSION = '0.01';
  @MyServer::ISA = qw(RPC::PlServer);

  # Overwrite the Run() method to handle a single connection
  sub Run {
      my $self = shift;
      my $socket = $self->{'socket'};
  }

  # Create an instance of the MyServer class
  package main;
  my $server = MyServer->new({'localport' => '1234'}, \@ARGV);

  # Bind the server to its port to make it actually running
  $server->Bind();


=head1 DESCRIPTION

PlRPC (Perl RPC) is a package for implementing servers and clients that
are written in Perl entirely. The name is borrowed from Sun's RPC
(Remote Procedure Call), but it could as well be RMI like Java's "Remote
Method Interface), because PlRPC gives you the complete power of Perl's
OO framework in a very simple manner.

RPC::PlServer is the package used on the server side, and you guess what
RPC::PlClient is for. Both share the package RPC::PlServer::Comm for
communication purposes. See L<PlRPC::Client(3)> and L<RPC::PlServer::Comm>
for these parts.

PlRPC works by defining a set of methods that may be executed by the client.
For example, the server might offer a method "multiply" to the client. Now
the clients method call

    @result = $client->multiply($a, $b);

will be immediately mapped to a method call

    @result = $server->multiply($a, $b);

on the server. The arguments and results will be transferred to or from
the server automagically. (This magic has a name in Perl: It's the
Storable module, my thanks to Raphael Manfredi for this excellent
package.) Simple, eh? :-)

The RPC::PlServer and RPC::PlClient are abstract servers and clients: You
have to derive your own classes from it.


=head2 Additional options

The RPC::PlServer inherits all of Net::Daemon's options and attributes
and adds the following:

=over 8

=item I<cipher>

The attribute value is an instance of Crypt::DES, Crypt::IDEA or any
other class with the same API for block encryption. If you supply
such an attribute, the traffic between client and server will be
encrypted using this option.

=item I<maxmessage> (B<--maxmessage=size>)

The size of messages exchanged between client and server is restricted,
in order to omit denial of service attacks. By default the limit is
65536 bytes.

=item users

This is an attribute of the client object used for Permit/Deny rules
in the config file. It's value is an array ref of user names that
are allowed to connect from the given client. See the example config
file below. L<CONFIGURATION FILE>.

=back

=head2 Error Handling

Error handling is simple with the RPC package, because it is based on
Perl exceptions completely. Thus your typical code looks like this:

  eval {
      # Do something here. Don't care for errors.
      ...
  };
  if ($@) {
      # An error occurred.
      ...
  }


=head2 Server Constructors

  my $server = RPC::PlServer(\%options, \@args);

(Class method) This constructor is immediately inherited from the
Net::Daemon package. See L<Net::Daemon(3)> for details.


=head2 Access Control

  $ok = $self->AcceptApplication($app);
  $ok = $self->AcceptVersion($version);
  $ok = $self->AcceptUser($user, $password);

The RPC::PlServer package has a very detailed access control scheme: First
of all it inherits Net::Daemon's host based access control. It adds
version control and user authorization. To achieve that, the method
I<Accept> from Net::Daemon is split into three methods,
I<AcceptApplication>, I<AcceptVersion> and I<AcceptUser>, each of them
returning TRUE or FALSE. The client receives the arguments as the attributes
I<application>, I<version>, I<user> and I<password>. A client is accepted
only if all of the above methods are returning TRUE.

The default implementations are as follows: The AcceptApplication method
returns TRUE, if B<$self> is a subclass of B<$app>. The AcceptVersion
method returns TRUE, if the requested version is less or equal to
B<${$class}::VERSION>, $self being an instance of B<$class>. Whether a user
is permitted to connect depends on the client configuration. See
L<CONFIGURATION FILE> below for examples.


=head2 Method based access control

Giving a client the ability to invoke arbitrary methods can be a terrible
security hole. Thus the server has a I<methods> attribute. This is a hash
ref of class names as keys, the values being hash refs again with method
names as the keys. That is, if your hash looks as follows:

    $self->{'methods'} = {
        'CalcServer' => {
            'NewHandle' => 1,
            'CallMethod' => 1 },
        'Calculator' => {
            'new' => 1,
            'multiply' => 1,
            'add' => 1,
            'divide' => 1,
            'subtract' => 1 }
        };

then the client may use the CalcServer's I<NewHandle> method to create
objects, but only via the permitted constructor Calculator->new. Once
a Calculator object is created, the server may invoke the methods
multiply, add, divide and subtract.


=head1 CONFIGURATION FILE

The server config file is inherited from Net::Daemon. It adds the
I<users> and I<cipher> attribute to the client list. Thus a typical
config file might look as follows:

    # Load external modules; this is not required unless you use
    # the chroot() option.
    #require DBD::mysql;
    #require DBD::CSV;

    # Create keys
    my $myhost_key = Crypt::IDEA->new('83fbd23390ade239');
    my $bob_key    = Crypt::IDEA->new('be39893df23f98a2');

    {
        # 'chroot' => '/var/dbiproxy',
        'facility' => 'daemon',
        'pidfile' => '/var/dbiproxy/dbiproxy.pid',
        'user' => 'nobody',
        'group' => 'nobody',
        'localport' => '1003',
        'mode' => 'fork',

        # Access control
        'clients' => [
            # Accept the local LAN (192.168.1.*)
            {
                'mask' => '^192\.168\.1\.\d+$',
                'accept' => 1,
                'users' => [ 'bob', 'jim' ],
                'cipher' => $myhost_key
            },
            # Accept myhost.company.com
            {
                'mask' => '^myhost\.company\.com$',
                'accept' => 1,
                'users' => [ {
                    'name' => 'bob',
                    'cipher' => $bob_key
                    } ]
            },
            # Deny everything else
            {
                'mask' => '.*',
                'accept' => 0
            }
        ]
    }

Things you should note: The user list of 192.168.1.* contains scalar
values, but the user list of myhost.company.com contains hash refs:
This is required, because the user configuration is more specific
for user based encryption.



=head1 EXAMPLE

Enough wasted time, spread the example, not the word. :-) Let's write
a simple server, say a server for MD5 digests. The server uses the
external package MD5, but the client doesn't need to install the
package. L<MD5(3)>. We present the server source here, the client
is part of the RPC::PlClient man page. See L<RPC::PlClient(3)>.


    #!/usr/bin/perl -wT
    # Note the -T switch! This is always recommended for Perl servers.

    use strict;               # Always a good choice.

    require RPC::PlServer;
    require MD5;


    package MD5_Server;  # Clients need to request application
                         # "MD5_Server"

    $MD5_Server::VERSION = '1.0'; # Clients will be refused, if they
                                  # request version 1.1
    @MD5_Server::ISA = qw(RPC::PlServer);

    eval {
        # Server options below can be overwritten in the config file or
        # on the command line.
        my $server = MD5_Server->new({
	    'pidfile'    => '/var/run/md5serv.pid',
	    'configfile' => '/etc/md5serv.conf',
	    'facility'   => 'daemon', # Default
	    'user'       => 'nobody',
	    'group'      => 'nobody',
	    'localport'  => 2000,
	    'logfile'    => 0,        # Use syslog
            'mode'       => 'fork',   # Recommended for Unix
            'methods'    => {
	        'MD5_Server' => {
		    'ClientObject' => 1,
		    'CallMethod' => 1,
		    'NewHandle' => 1
		    },
	        'MD5' => {
		    'new' => 1,
		    'add' => 1,
		    'hexdigest' => 1
		    },
	        }
        });
        $server->Bind();
    };


=head1 SECURITY

It has to be said: PlRPC based servers are a potential security problem!
I did my best to avoid security problems, but it is more than likely,
that I missed something. Security was a design goal, but not *the*
design goal. (A well known problem ...)

I highly recommend the following design principles:

=head2 Protection against "trusted" users

=over 4

=item perlsec

Read the perl security FAQ (C<perldoc perlsec>) and use the C<-T> switch.

=item taintperl

B<Use> the C<-T> switch. I mean it!

=item Verify data

Never untaint strings withouth verification, better verify twice.
For example the I<CallMethod> function first checks, whether an
object handle is valid before coercing a method on it.

=item Be restrictive

Think twice, before you give a client access to a method.

=item perlsec

And just in case I forgot it: Read the C<perlsec> man page. :-)

=back

=head2 Protection against untrusted users

=over 4

=item Host based authorization

PlRPC has a builtin host based authorization scheme; use it!
See L</CONFIGURATION FILE>.

=item User based authorization

PlRPC has a builtin user based authorization scheme; use it!
See L</CONFIGURATION FILE>.


=item Encryption

Using encryption with PlRPC is extremely easy. There is absolutely
no reason for communicating unencrypted with the clients. Even
more: I recommend two phase encryption: The first phase is the
login phase, where to use a host based key. As soon as the user
has authorized, you should switch to a user based key. See the
DBI::ProxyServer for an example.

=back

=head1 AUTHOR AND COPYRIGHT

The PlRPC-modules are

  Copyright (C) 1998, Jochen Wiedmann
                      Email: jochen.wiedmann at freenet.de

  All rights reserved.

You may distribute this package under the terms of either the GNU
General Public License or the Artistic License, as specified in the
Perl README file.


=head1 SEE ALSO

L<RPC::PlClient(3)>, L<RPC::PlServer::Comm(3)>, L<Net::Daemon(3)>,
L<Net::Daemon::Log(3)>, L<Storable(3)>, L<Sys::Syslog(3)>,
L<Win32::EventLog(3)>

See L<DBI::ProxyServer(3)> for an example application.

=cut
