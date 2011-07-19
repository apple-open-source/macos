#   -*- perl -*-
#
#
#   PlRPC - Perl RPC, package for writing simple, RPC like clients and
#       servers
#
#   RPC::PlClient.pm is the module for writing the PlRPC client.
#
#
#   Copyright (c) 1997, 1998  Jochen Wiedmann
#
#   You may distribute under the terms of either the GNU General Public
#   License or the Artistic License, as specified in the Perl README file.
#
#   Author: Jochen Wiedmann
#           Email: jochen.wiedmann at freenet.de
#

use strict;

use RPC::PlClient::Comm ();
use Net::Daemon::Log ();
use IO::Socket ();


package RPC::PlClient;

$RPC::PlClient::VERSION = '0.2020';
@RPC::PlClient::ISA = qw(Net::Daemon::Log);


############################################################################
#
#   Name:    new
#
#   Purpose: Constructor of the PlRPC::Client module
#
#   Inputs:  $self - Class name
#            @attr - Attribute list
#
#   Returns: Client object; dies in case of errors.
#
############################################################################

sub new ($@) {
    my $proto = shift;
    my $self = {@_};
    bless($self, (ref($proto) || $proto));

    my $comm = $self->{'comm'} = RPC::PlClient::Comm->new($self);
    my $app = $self->{'application'}  or
	$self->Fatal("Missing application name");
    my $version = $self->{'version'}  or
	$self->Fatal("Missing version number");
    my $user = $self->{'user'} || '';
    my $password = $self->{'password'} || '';

    my $socket;
    if (!($socket = $self->{'socket'})) {
	$self->Fatal("Missing peer address") unless $self->{'peeraddr'};
	$self->Fatal("Missing peer port")
	    unless ($self->{'peerport'}  ||
		    index($self->{'peeraddr'}, ':') != -1);
	$socket = $self->{'socket'} = IO::Socket::INET->new
	    ('PeerAddr' => $self->{'peeraddr'},
	     'PeerPort' => $self->{'peerport'},
	     'Proto'    => $self->{'socket_proto'},
	     'Type'     => $self->{'socket_type'},
	     'Timeout'  => $self->{'timeout'});
	$self->Fatal("Cannot connect: $!") unless $socket;
    }
    $self->Debug("Connected to %s, port %s",
		 $socket->peerhost(), $socket->peerport());
    $self->Debug("Sending login message: %s, %s, %s, %s",
		 $app, $version, $user, "x" x length($password));
    $comm->Write($socket, [$app, $version, $user, $password]);
    $self->Debug("Waiting for server's response ...");
    my $reply = $comm->Read($socket);
    die "Unexpected EOF from server" unless defined($reply);
    die "Expected server to return an array ref" unless ref($reply) eq 'ARRAY';
    my $msg = defined($reply->[1]) ? $reply->[1] : '';
    die "Refused by server: $msg" unless $reply->[0];
    $self->Debug("Logged in, server replies: $msg");

    return ($self, $msg) if wantarray;
    $self;
}


############################################################################
#
#   Name:    Call
#
#   Purpose: Coerce method located on the server
#
#   Inputs:  $self - client instance
#            $method - method name
#            @args - method attributes
#
#   Returns: method results; dies in case of errors.
#
############################################################################

sub Call ($@) {
    my $self = shift;
    my $socket = $self->{'socket'};
    my $comm = $self->{'comm'};
    $comm->Write($socket, [@_]);
    my $msg = $comm->Read($socket);
    die "Unexpected EOF while waiting for server reply" unless defined($msg);
    die "Server returned error: $$msg" if ref($msg) eq 'SCALAR';
    die "Expected server to return an array ref" unless ref($msg) eq 'ARRAY';
    @$msg;
}

sub ClientObject {
    my $client = shift;  my $class = shift;  my $method = shift;
    my($object) = $client->Call('NewHandle', $class, $method, @_);
    die "Constructor didn't return a TRUE value" unless $object;
    die "Constructor didn't return an object"
	unless $object =~ /^((?:\w+|\:\:)+)=(\w+)/;
    RPC::PlClient::Object->new($1, $client, $object);
}

sub Disconnect {
    my $self = shift;
    $self->{'socket'} = undef;
    1;
}


package RPC::PlClient::Object;

use vars qw($AUTOLOAD);

sub AUTOLOAD {
    my $method = $AUTOLOAD;
    my $index;
    die "Cannot parse method: $method"
	unless ($index = rindex($method, '::')) != -1;
    my $class = substr($method, 0, $index);
    $method = substr($method, $index+2);
    eval <<"EOM";
        package $class;
        sub $method {
            my \$self = shift;
            my \$client = \$self->{'client'}; my \$object = \$self->{'object'};
            my \@result = \$client->Call('CallMethod', \$object, '$method',
					 \@_);
            return \@result if wantarray;
            return \$result[0];
        }
EOM
    goto &$AUTOLOAD;
}

sub new {
    my($class, $cl, $client, $object) = @_;
    $class = ref($class) if ref($class);
    no strict 'refs';
    my $ocl = "${class}::$cl";
    @{"${ocl}::ISA"} = $class unless @{"${ocl}::ISA"};
    my $self = { 'client' => $client, 'object' => $object };

    bless($self, $ocl);
    $self;
}


sub DESTROY {
    my $saved_error = $@; # Save $@
    my $self = shift;
    if (my $client = delete $self->{'client'}) {
	eval { $client->Call('DestroyHandle', $self->{'object'}) };
    }
    $@ = $saved_error;    # Restore $@
}

1;


__END__


=pod

=head1 NAME

RPC::PlClient - Perl extension for writing PlRPC clients


=head1 SYNOPSIS

  require RPC::PlClient;

  # Create a client object and connect it to the server
  my $client = RPC::PlClient->new('peeraddr' => 'joes.host.de',
				  'peerport' => 2570,
				  'application' => 'My App',
				  'version' => '1.0',
				  'user' => 'joe',
				  'password' => 'hello!');

  # Create an instance of $class on the server by calling $class->new()
  # and an associated instance on the client.
  my $object = $client->Call('NewHandle', $class, 'new', @args);


  # Call a method on $object, effectively calling the same method
  # on the associated server instance.
  my $result = $object->do_method(@args);


=head1 DESCRIPTION

PlRPC (Perl RPC) is a package that simplifies the writing of
Perl based client/server applications. RPC::PlServer is the
package used on the server side, and you guess what RPC::PlClient
is for. See L<RPC::PlServer(3)> for this part.

PlRPC works by defining a set of methods that may be executed by the client.
For example, the server might offer a method "multiply" to the client. Now
a function call

    @result = $client->Call('multiply', $a, $b);

on the client will be mapped to a corresponding call

    $server->multiply($a, $b);

on the server. The function calls result will be transferred to the
client and returned as result of the clients method. Simple, eh? :-)


=head2 Client methods

=over 4

=item $client = new(%attr);

(Class method) The client constructor. Returns a client object, connected
to the server. A Perl exception is thrown in case of errors, thus you
typically use it like this:

    $client = eval { RPC::PlClient->new ( ... ) };
    if ($@) {
	print STDERR "Cannot create client object: $@\n";
	exit 0;
    }

The method accepts a list of key/value pairs as arguments. Known arguments
are:

=over 8

=item peeraddr

=item peerport

=item socket_proto

=item socket_type

=item timeout

These correspond to the attributes I<PeerAddr>, I<PeerPort>, I<Proto>,
I<Type> and I<Timeout> of IO::Socket::INET. The server connection will be
established by passing them to IO::Socket::INET->new().

=item socket

After a connection was established, the IO::Socket instance will be stored
in this attribute. If you prefer establishing the connection on your own,
you may as well create an own instance of IO::Socket and pass it as attribute
I<socket> to the new method. The above attributes will be ignored in that
case.

=item application

=item version

=item user

=item password

it is part of the PlRPC authorization process, that the client
must obeye a login procedure where he will pass an application
name, a protocol version and optionally a user name and password.
These arguments are handled by the servers I<Application>, I<Version>
and I<User> methods.

=item compression

Set this to off (default, no compression) or gzip (requires the
Compress::Zlib module).

=item cipher

This attribute can be used to add encryption quite easily. PlRPC is not
bound to a certain encryption method, but to a block encryption API. The
attribute is an object supporting the methods I<blocksize>, I<encrypt>
and I<decrypt>. For example, the modules Crypt::DES and Crypt::IDEA
support such an interface.

Note that you can set or remove encryption on the fly (putting C<undef>
as attribute value will stop encryption), but you have to be sure,
that both sides change the encryption mode.

Example:

    use Crypt::DES;
    $cipher = Crypt::DES->new(pack("H*", "0123456789abcdef"));
    $client = RPC::PlClient->new('cipher' => $cipher,
				...);

=item maxmessage

The size of messages exchanged between client and server is restricted,
in order to omit denial of service attacks. By default the limit is
65536 bytes.

=item debug

Enhances logging level by emitting debugging messages.

=item logfile

By default the client is logging to syslog (Unix) or the event log (Windows).
If neither is available or you pass a TRUE value as I<logfile>, then logging
will happen to the given file handle, an instance of IO::Handle. If the
value is scalar, then logging will occur to stderr.

Examples:

  # Logging to stderr:
  my $client = RPC::PlClient->new('logfile' => 1, ...);

  # Logging to 'my.log':
  my $file = IO::File->new('my.log', 'a')
      || die "Cannot create log file 'my.log': $!";
  my $client = RPC::PlClient->new('logfile' => $file, ...);

=back

=item @result = $client->Call($method, @args);

(Instance method) Calls a method on the server; the arguments are a method
name of the server class and the method call arguments. It returns the
method results, if successfull, otherwise a Perl exception is thrown.

Example:

  @results = eval { $client->Call($method, @args };
  if ($@) {
      print STDERR "An error occurred while executing $method: $@\n";
      exit 0;
  }

=item $cobj = $client->ClientObject($class, $method, @args)

(Instance method) A set of predefined methods is available that make
dealing with client side objects incredibly easy: In short the client
creates a representation of the server object for you. Say we have an
object $sobj on the server and an associated object $cobj on the client:
Then a call

  @results = $cobj->my_method(@args);

will be immediately mapped to a call

  @results = $sobj->my_method(@args);

on the server and the results returned to you without any additional
programming. Here's how you create $cobj, an instance of
I<RPC::PlClient::Object>:

  my $cobj = $client->ClientObject($class, 'new', @args);

This will trigger a call

  my $sobj = $class->new(@args);

on the server for you. Note that the server has the ability to restrict
access to both certain classes and methods by setting $server->{'methods'}
appropriately.

=back


=head1 EXAMPLE

We'll create a simple example application, an MD5 client. The server
will have installed the MD5 module and create digests for us. We
present the client part only, the server example is part of the
RPC::PlServer man page. See L<RPC::PlServer(3)>.

    #!/usr/local/bin/perl

    use strict;               # Always a good choice.

    require RPC::PlClient;

    # Constants
    my $MY_APPLICATION = "MD5_Server";
    my $MY_VERSION = 1.0;
    my $MY_USER = "";		# The server doesn't require user
    my $MY_PASSWORD = "";	# authentication.

    my $hexdigest = eval {
        my $client = RPC::PlClient->new
	    ('peeraddr'    => '127.0.0.1',
	     'peerport'    => 2000,
	     'application' => $MY_APPLICATION,
	     'version'     => $MY_VERSION,
	     'user'        => $MY_USER,
	     'password'    => $MY_PASSWORD);

        # Create an MD5 object on the server and an associated
        # client object. Executes a
        #     $context = MD5->new()
        # on the server.
        my $context = $client->ClientObject('MD5', 'new');

        # Let the server calculate a digest for us. Executes a
        #     $context->add("This is a silly string!");
        #     $context->hexdigest();
        # on the server.
	$context->add("This is a silly string!");
	$context->hexdigest();
    };
    if ($@) {
        die "An error occurred: $@";
    }

    print "Got digest $hexdigest\n";


=head1 AUTHOR AND COPYRIGHT

The PlRPC-modules are

  Copyright (C) 1998, Jochen Wiedmann
                      Email: jochen.wiedmann at freenet.de

  All rights reserved.

You may distribute this package under the terms of either the GNU
General Public License or the Artistic License, as specified in the
Perl README file.


=head1 SEE ALSO

L<PlRPC::Server(3)>, L<Net::Daemon(3)>, L<Storable(3)>, L<Sys::Syslog(3)>,
L<Win32::EventLog>

An example application is the DBI Proxy client:

L<DBD::Proxy(3)>.

=cut

