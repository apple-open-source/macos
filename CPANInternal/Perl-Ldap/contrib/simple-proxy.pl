#!/usr/bin/perl
# Copyright (c) 2006 Hans Klunder <hans.klunder@bigfoot.com>. All rights reserved.
# This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.


use strict;
use warnings;

use IO::Select;
use IO::Socket;
use Data::Dumper;
use Convert::ASN1 qw(asn_read);
use Net::LDAP::ASN qw(LDAPRequest LDAPResponse);
our $VERSION = '0.1';
use fields qw(socket target);


sub handle($$)
{
  my $clientsocket = shift;
  my $serversocket = shift;

  # read from client
  asn_read($clientsocket, my $reqpdu);
  log_request($reqpdu);
  
  # send to server
  print $serversocket $reqpdu or die "Could not send PDU to server\n";
  
  # read from server
  my $ready;
  my $sel = IO::Select->new($serversocket);
  for( $ready = 1 ; $ready ; $ready = $sel->can_read(0)) {
    asn_read($serversocket, my $respdu) or return 1;
    log_response($respdu);
    # and send the result to the client
    print $clientsocket $respdu;
  }

  return 0;
}


sub log_request($)
{
  my $pdu = shift;

  print '-' x 80,"\n";
  print "Request ASN 1:\n";
  Convert::ASN1::asn_hexdump(\*STDOUT,$pdu);
  print "Request Perl:\n";
  my $request = $LDAPRequest->decode($pdu);
  print Dumper($request);
}


sub log_response($)
{
  my $pdu = shift;

  print '-' x 80,"\n";
  print "Response ASN 1:\n";
  Convert::ASN1::asn_hexdump(\*STDOUT,$pdu);
  print "Response Perl:\n";
  my $response = $LDAPResponse->decode($pdu);
  print Dumper($response);
}


sub run_proxy($$)
{
  my $listenersock = shift;
  my $targetsock = shift;

  return unless ($listenersock && $targetsock);
  
  my $sel = IO::Select->new($listenersock);
  my %Handlers;
  while (my @ready = $sel->can_read) {
    foreach my $fh (@ready) {
      if ($fh == $listenersock) {
	# let's create a new socket
	my $psock = $listenersock->accept;
	$sel->add($psock);
      } else {
	my $result = handle($fh,$targetsock);
	if ($result) {
	  # we have finished with the socket
	  $sel->remove($fh);
	  $fh->close;
	  delete $Handlers{*$fh};
	}
      }
    }
  }
}


my $listenersock = IO::Socket::INET->new(
	Listen => 5,
	Proto => 'tcp',
	Reuse => 1,
	LocalPort => 7070 )
  or  die "Could not create listener socket: $!\n";	


my $targetsock = IO::Socket::INET->new(
  	Proto => 'tcp',
	  PeerAddr => 'localhost',
	  PeerPort => 8080 )
  or  die "Could not create connection to server: $!\n";

run_proxy($listenersock,$targetsock);

1;

__END__


Hi,

I noticed in the TODO that there was a request for a simple proxy which 
can act as a man-in-the-middle.

Well, the attached script provides such a proxy, it is really a simple 
proxy as it can currently handle only one client at the time, it will 
dump requests and responses to STDOUT both in ASN1 and as perl structure.

Cheers,

Hans
ps. If you need a little more power like returning entries on a query I 
suggest to have a look at Net::LDAP::Server on CPAN.

# EOF
