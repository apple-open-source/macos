#!/usr/bin/perl -w -T

###----------------------------------------###
###     sample udp server class            ###
###----------------------------------------###
package MyUDPD;
use lib qw(/home/rhandom/Net-Server/lib);
use vars qw(@ISA);
use strict;

### what type of server is this - we could
### use multi type when we add command line
### parsing to this http server to allow
### for different configurations
use Net::Server::PreFork;
@ISA = qw(Net::Server::PreFork);

### run the server
MyUDPD->run( port => '20203/udp',
             # we could also do the following:
             # port => '*:20203/udp',
             # port => 'somehost:20203/udp',
             # port => '20203/udp', port => '20204/udp',
             # port => '20203/udp', port => '20203/tcp',
             );
exit;



### set up some server parameters
sub configure_hook {
  my $self = shift;

  ### change the packet len?
  # $self->{server}->{udp_recv_len} ||= 2048; # default is 4096

}




### this is the main method to override
### this is where most of the work will occur
### A sample server is shown below.
sub process_request {
  my $self = shift;
  my $prop = $self->{server};

  ### if we were writing a server that did both tcp and udp,
  ### we would need to check $prop->{udp_true} to see
  ### if the current connection is udp or not

#  if( $prop->{udp_true} ){
#    # yup, this is udp
#  }
 
  if( $prop->{udp_data} =~ /dump/ ){
    require "Data/Dumper.pm";
    $prop->{client}->send( Data::Dumper::Dumper( $self ) , 0);
  }else{
    $prop->{client}->send( "You said \"$prop->{udp_data}\"", 0);
  }
  return;

}
