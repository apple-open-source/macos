#!/usr/bin/perl -Tw
##
##  loadcacert.cgi -- Load a CA certificate into Communicator
##  Copyright (c) 1998-2001 Ralf S. Engelschall, All Rights Reserved. 
##

use strict;

$|++;

open(FP, "<ca.crt");
my $cert = '';
$cert .= $_ while (<FP>);
close(FP);
my $len = length($cert);

print "Content-type: application/x-x509-ca-cert\r\n";
print "Content-length: $len\r\n";
print "\r\n";
print $cert;

