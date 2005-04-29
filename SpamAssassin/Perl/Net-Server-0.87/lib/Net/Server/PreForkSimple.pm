# -*- perl -*-
#
#  Net::Server::PreForkSimple - Net::Server personality
#  
#  $Id: PreForkSimple.pm,v 1.1 2004/04/19 17:50:29 dasenbro Exp $
#  
#  Copyright (C) 2001, Paul T Seamons
#                      paul@seamons.com
#                      http://seamons.com/
#
#  This package may be distributed under the terms of either the
#  GNU General Public License
#    or the
#  Perl Artistic License
#
#  All rights reserved.
#
################################################################

package Net::Server::PreForkSimple;

use strict;
use vars qw($VERSION @ISA $LOCK_EX $LOCK_UN);
use POSIX qw(WNOHANG);
use Fcntl ();
use Net::Server ();
use Net::Server::SIG qw(register_sig check_sigs);

$VERSION = $Net::Server::VERSION; # done until separated

### fall back to parent methods
@ISA = qw(Net::Server);

### override-able options for this package
sub options {
  my $self = shift;
  my $prop = $self->{server};
  my $ref  = shift;

  $self->SUPER::options($ref);

  foreach ( qw(max_servers max_requests max_dequeue
               check_for_dead check_for_dequeue
               lock_file serialize) ){
    $prop->{$_} = undef unless exists $prop->{$_};
    $ref->{$_} = \$prop->{$_};
  }

}

### make sure some defaults are set
sub post_configure {
  my $self = shift;
  my $prop = $self->{server};

  ### let the parent do the rest
  ### must do this first so that ppid reflects backgrounded process
  $self->SUPER::post_configure;

  ### some default values to check for
  my $d = {max_servers       => 50,   # max num of servers to run
           max_requests      => 1000, # num of requests for each child to handle
           check_for_dead    => 30,   # how often to see if children are alive
           };
  foreach (keys %$d){
    $prop->{$_} = $d->{$_}
    unless defined($prop->{$_}) && $prop->{$_} =~ /^\d+$/;
  }

  ### I need to know who is the parent
  $prop->{ppid} = $$;
}


### now that we are bound, prepare serialization
sub post_bind {
  my $self = shift;
  my $prop = $self->{server};

  ### do the parents
  $self->SUPER::post_bind;

  ### clean up method to use for serialization
  if( ! defined($prop->{serialize})
      || $prop->{serialize} !~ /^(flock|semaphore|pipe)$/i ){
    $prop->{serialize} = 'flock';
  }
  $prop->{serialize} =~ tr/A-Z/a-z/;

  ### set up lock file
  if( $prop->{serialize} eq 'flock' ){
    $self->log(3,"Setting up serialization via flock");
    if( defined($prop->{lock_file}) ){
      $prop->{lock_file_unlink} = undef;
    }else{
      $prop->{lock_file} = POSIX::tmpnam();
      $prop->{lock_file_unlink} = 1;
    }

  ### set up semaphore
  }elsif( $prop->{serialize} eq 'semaphore' ){
    $self->log(3,"Setting up serialization via semaphore");
    require "IPC/SysV.pm";
    require "IPC/Semaphore.pm";
    my $s = IPC::Semaphore->new(IPC::SysV::IPC_PRIVATE(),
                                1,
                                IPC::SysV::S_IRWXU() | IPC::SysV::IPC_CREAT(),
                                ) || $self->fatal("Semaphore error [$!]");
    $s->setall(1) || $self->fatal("Semaphore create error [$!]");
    $prop->{sem} = $s;

  ### set up pipe
  }elsif( $prop->{serialize} eq 'pipe' ){
    pipe( _WAITING, _READY );
    _READY->autoflush(1);
    _WAITING->autoflush(1);
    $prop->{_READY}   = *_READY;
    $prop->{_WAITING} = *_WAITING;
    print _READY "First\n";

  }else{
    $self->fatal("Unknown serialization type \"$prop->{serialize}\"");
  }

}

### prepare for connections
sub loop {
  my $self = shift;
  my $prop = $self->{server};

  ### get ready for children
  $prop->{children} = {};

  $self->log(3,"Beginning prefork ($prop->{max_servers} processes)\n");

  ### start up the children
  $self->run_n_children( $prop->{max_servers} );

  ### finish the parent routines
  $self->run_parent;

}

### subroutine to start up a specified number of children
sub run_n_children {
  my $self  = shift;
  my $prop  = $self->{server};
  my $n     = shift;
  return unless $n > 0;

  $self->log(3,"Starting \"$n\" children");

  for( 1..$n ){
    my $pid = fork;

    ### trouble
    if( not defined $pid ){
      $self->fatal("Bad fork [$!]");

    ### parent
    }elsif( $pid ){
      $prop->{children}->{$pid}->{status} = 'processing';

    ### child
    }else{
      $self->run_child;

    }
  }
}

### child process which will accept on the port
sub run_child {
  my $self = shift;
  my $prop = $self->{server};

  ### restore sigs (turn off warnings during)
  $SIG{INT} = $SIG{TERM} = $SIG{QUIT}
    = $SIG{CHLD} = sub {
      $self->child_finish_hook;
      exit;
    };

  $self->log(4,"Child Preforked ($$)\n");

  $self->child_init_hook;

  ### let the parent shut me down
  $prop->{connected} = 0;
  $prop->{SigHUPed}  = 0;
  $SIG{HUP} = sub {
    unless( $prop->{connected} ){
      $self->child_finish_hook;
      exit;
    }
    $prop->{SigHUPed} = 1;
  };

  ### accept connections
  while( $self->accept() ){

    $prop->{connected} = 1;

    $self->run_client_connection;

    last if $self->done;

    $prop->{connected} = 0;

  }

  $self->child_finish_hook;

  $self->log(4,"Child leaving ($prop->{max_requests})");
  exit;

}

### hooks at the beginning and end of forked child processes
sub child_init_hook {}
sub child_finish_hook {}



### We can only let one process do the selecting at a time
### this override makes sure that nobody else can do it
### while we are.  We do this either by opening a lock file
### and getting an exclusive lock (this will block all others
### until we release it) or by using semaphores to block
sub accept {
  my $self = shift;
  my $prop = $self->{server};

  local *LOCK;

  ### serialize the child accepts
  if( $prop->{serialize} eq 'flock' ){
    open(LOCK,">$prop->{lock_file}")
      || $self->fatal("Couldn't open lock file \"$prop->{lock_file}\" [$!]");
    flock(LOCK,Fcntl::LOCK_EX())
      || $self->fatal("Couldn't get lock on file \"$prop->{lock_file}\" [$!]");

  }elsif( $prop->{serialize} eq 'semaphore' ){
    $prop->{sem}->op( 0, -1, IPC::SysV::SEM_UNDO() )
      || $self->fatal("Semaphore Error [$!]");

  }elsif( $prop->{serialize} eq 'pipe' ){
    scalar <_WAITING>; # read one line - kernel says who gets it
  }


  ### now do the accept method
  my $accept_val = $self->SUPER::accept();


  ### unblock serialization
  if( $prop->{serialize} eq 'flock' ){
    flock(LOCK,Fcntl::LOCK_UN());

  }elsif( $prop->{serialize} eq 'semaphore' ){
    $prop->{sem}->op( 0, 1, IPC::SysV::SEM_UNDO() )
      || $self->fatal("Semaphore Error [$!]");

  }elsif( $prop->{serialize} eq 'pipe' ){
    print _READY "Next!\n";
  }

  ### return our success
  return $accept_val;

}


### is the looping done (non zero value says its done)
sub done {
  my $self = shift;
  my $prop = $self->{server};
  return 1 if $prop->{requests} >= $prop->{max_requests};
  return 1 if $prop->{SigHUPed};
  if( ! kill(0,$prop->{ppid}) ){
    $self->log(3,"Parent process gone away. Shutting down");
    return 1;
  }
}


### now the parent will wait for the kids
sub run_parent {
  my $self=shift;
  my $prop = $self->{server};

  $self->log(4,"Parent ready for children.\n");

  ### set some waypoints
  $prop->{last_checked_for_dead}
  = $prop->{last_checked_for_dequeue}
  = time();

  ### register some of the signals for safe handling
  register_sig(PIPE => 'IGNORE',
               INT  => sub { $self->server_close() },
               TERM => sub { $self->server_close() },
               QUIT => sub { $self->server_close() },
               HUP  => sub { $self->sig_hup() },
               CHLD => sub {
                 while ( defined(my $chld = waitpid(-1, WNOHANG)) ){
                   last unless $chld > 0;
                   $self->delete_child($chld);
                 }
               },
### uncomment this area to allow SIG USR1 to give some runtime debugging
#               USR1 => sub {
#                 require "Data/Dumper.pm";
#                 print Data::Dumper::Dumper($self);
#               },
               );

  ### loop forever
  while( 1 ){

    ### sleep up to 10 seconds
    select(undef,undef,undef,10);

    ### check for any signals
    my @sigs = &check_sigs();
    if( @sigs ){
      last if $prop->{_HUP};
    }

    my $time = time();

    ### periodically make sure children are alive
    if( $time - $prop->{last_checked_for_dead} > $prop->{check_for_dead} ){
      $prop->{last_checked_for_dead} = $time;
      foreach (keys %{ $prop->{children} }){
        ### see if the child can be killed
        kill(0,$_) or $self->delete_child($_);
      }
    }

    ### make sure we always have max_servers
    my $total_n = 0;
    my $total_d = 0;
    foreach (values %{ $prop->{children} }){
      if( $_->{status} eq 'dequeue' ){
        $total_d ++;
      }else{
        $total_n ++;
      }
    }

    if( $prop->{max_servers} > $total_n ){
      $self->run_n_children( $prop->{max_servers} - $total_n );
    }

    ### periodically check to see if we should clear the queue
    if( defined $prop->{check_for_dequeue} ){
      if( $time - $prop->{last_checked_for_dequeue}
          > $prop->{check_for_dequeue} ){
        $prop->{last_checked_for_dequeue} = $time;
        if( defined($prop->{max_dequeue})
            && $total_d < $prop->{max_dequeue} ){
          $self->run_dequeue();
        }
      }
    }

  }

  ### allow fall back to main run method

}

### Stub function in case check_for_dequeue is used.
sub run_dequeue {
  die "run_dequeue: virtual method not defined";
}

1;

__END__

=head1 NAME

Net::Server::PreForkSimple - Net::Server personality

=head1 SYNOPSIS

  use Net::Server::PreForkSimple;
  @ISA = qw(Net::Server::PreFork);

  sub process_request {
     #...code...
  }

  __PACKAGE__->run();

=head1 DESCRIPTION

Please read the pod on Net::Server first.  This module
is a personality, or extension, or sub class, of the
Net::Server module.

This personality binds to one or more ports and then forks
C<max_servers> child processes.  The server will make sure
that at any given time there are always C<max_servers>
available to receive a client request.  Each of
these children will process up to C<max_requests> client
connections.  This type is good for a heavily hit site that can
keep C<max_servers> processes dedicated to the serving.
(Multi port accept defaults to using flock to serialize the
children).

=head1 SAMPLE CODE

Please see the sample listed in Net::Server.

=head1 COMMAND LINE ARGUMENTS

In addition to the command line arguments of the Net::Server
base class, Net::Server::PreFork contains several other
configurable parameters.

  Key               Value                   Default
  max_servers       \d+                     50
  max_requests      \d+                     1000

  serialize         (flock|semaphore|pipe)  undef
  # serialize defaults to flock on multi_port or on Solaris
  lock_file         "filename"              POSIX::tmpnam

  check_for_dead    \d+                     30

  max_dequeue       \d+                     undef
  check_for_dequeue \d+                     undef

=over 4

=item max_servers

The maximum number of child servers to start and maintain.
This does not apply to dequeue processes.

=item max_requests

The number of client connections to receive before a
child terminates.

=item serialize

Determines whether the server serializes child connections.
Options are undef, flock, semaphore, or pipe.  Default is undef.
On multi_port servers or on servers running on Solaris, the
default is flock.  The flock option uses blocking exclusive
flock on the file specified in I<lock_file> (see below).
The semaphore option uses IPC::Semaphore (thanks to Bennett
Todd) for giving some sample code.  The pipe option reads on a
pipe to choose the next.  the flock option should be the
most bulletproof while the pipe option should be the most
portable.  (Flock is able to reliquish the block if the
process dies between accept on the socket and reading
of the client connection - semaphore and pipe do not)

=item lock_file

Filename to use in flock serialized accept in order to
serialize the accept sequece between the children.  This
will default to a generated temporary filename.  If default
value is used the lock_file will be removed when the server
closes.

=item check_for_dead

Seconds to wait before checking to see if a child died
without letting the parent know.

=item max_dequeue

The maximum number of dequeue processes to start.  If a
value of zero or undef is given, no dequeue processes will
be started.  The number of running dequeue processes will
be checked by the check_for_dead variable.

=item check_for_dequeue

Seconds to wait before forking off a dequeue process.  The
run_dequeue hook must be defined when using this setting.
It is intended to use the dequeue process to take care of
items such as mail queues.  If a value of undef is given,
no dequeue processes will be started.


=back

=head1 CONFIGURATION FILE

C<Net::Server::PreFork> allows for the use of a
configuration file to read in server parameters.  The format
of this conf file is simple key value pairs.  Comments and
white space are ignored.

  #-------------- file test.conf --------------

  ### server information
  max_servers   80

  max_requests  1000

  ### user and group to become
  user        somebody
  group       everybody

  ### logging ?
  log_file    /var/log/server.log
  log_level   3
  pid_file    /tmp/server.pid

  ### access control
  allow       .+\.(net|com)
  allow       domain\.com
  deny        a.+

  ### background the process?
  background  1

  ### ports to bind
  host        127.0.0.1
  port        localhost:20204
  port        20205

  ### reverse lookups ?
  # reverse_lookups on

  #-------------- file test.conf --------------

=head1 PROCESS FLOW

Process flow follows Net::Server until the loop phase.  At
this point C<max_servers> are forked and wait for
connections.  When a child accepts a connection, finishs
processing a client, or exits, it relays that information to
the parent, which keeps track and makes sure there are
always C<max_servers> running.

=head1 HOOKS

The PreForkSimple server has the following hooks in addition
to the hooks provided by the Net::Server base class.
See L<Net::Server>

=over 4

=item C<$self-E<gt>child_init_hook()>

This hook takes place immeditately after the child process
forks from the parent and before the child begins
accepting connections.  It is intended for any addiotional
chrooting or other security measures.  It is suggested
that all perl modules be used by this point, so that
the most shared memory possible is used.

=item C<$self-E<gt>child_finish_hook()>

This hook takes place immediately before the child tells
the parent that it is exiting.  It is intended for
saving out logged information or other general cleanup.

=item C<$self-E<gt>run_dequeue()>

This hook only gets called in conjuction with the
check_for_dequeue setting.

=back

=head1 TO DO

See L<Net::Server>

=head1 AUTHOR

Paul T. Seamons paul@seamons.com

=head1 THANKS

See L<Net::Server>

=head1 SEE ALSO

Please see also
L<Net::Server::Fork>,
L<Net::Server::INET>,
L<Net::Server::PreFork>,
L<Net::Server::MultiType>,
L<Net::Server::Single>
L<Net::Server::SIG>
L<Net::Server::Daemonize>
L<Net::Server::Proto>

=cut

