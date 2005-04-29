#!/usr/bin/perl -w

#use Time::HiRes qw(time);  # uncomment for more accuracy
use vars qw(@ISA);
use strict;
use Net::Server::PreFork;
use IO::Socket;

@ISA = qw(Net::Server::PreFork);
main->run();
exit;

$|=1;

### set up the test parameters
sub configure_hook {
  my $self = shift;
  $self->{addr} = 'localhost';   # choose a remote addr
  $self->{port} = 20203;         # choose a remote port
  $self->{failed} = 0;           # failed hits (server was blocked)
  $self->{hits} = 0;             # log hits
  $self->{max_hits}   = 1000;    # how many impressions to do
  $self->{time_begin} = time;    # keep track of time
  $self->{sleep} = 0;            # sleep between hits?
}


### these generally deal with sockets - ignore them
sub pre_bind {}
sub bind { shift()->log(2,"Running under pid $$\n") }
sub accept { 1 }
sub post_accept {}
sub get_client_info {}
sub allow_deny { 1 }
sub post_process_request {}


### connect to the remote server and get some information
sub process_request {
  my $self = shift;
  sleep($self->{sleep}) if $self->{sleep};

  ### try to connect and deliver the load
  if( $self->{remote} = IO::Socket::INET->new(PeerAddr => $self->{addr},
                                              PeerPort => $self->{port},
                                              Proto    => 'tcp') ){
    $self->load();
    return;

  }

  ### couldn't connect
  *_WRITE = $self->{server}->{_WRITE};
  _WRITE->autoflush(1);
  print _WRITE "failed [$!]\n";
}


### this is the test of the remote server
sub load {
  my $self = shift;
  my $handle = $self->{remote};
  $handle->autoflush(1);
  my $line = <$handle>;
  print $handle "quit\n";
}
 

### keep track of what is going on
sub parent_read_hook {
  my $self   = shift;
  my $status = shift;

  if( $status =~ /failed/i ){
    $self->{failed}++;
    print $status;
    $self->{hits}--;
  }
  return '' unless   $status =~/processing/i;
  print "*" unless $self->{hits} % 100;
  return '' unless ++$self->{hits} >= $self->{max_hits};

  $self->{time_end} = time;
  $self->print_report();

  return 1;
}


### spit out what happened
sub print_report {
  my $self = shift;
  my $time = $self->{time_end} - $self->{time_begin};

#  require "Data/Dumper.pm";
#  print STDERR Data::Dumper::Dumper( $self );
  print "\n$0 Results\n";
  print "--------------------------------------------\n";
  print "($self->{hits}) hits in ($time) seconds\n";
  print "($self->{failed}) failed hits\n";
  printf "%.3f hits per second\n", ($time ? $self->{hits}/$time : $self->{hits});
}
