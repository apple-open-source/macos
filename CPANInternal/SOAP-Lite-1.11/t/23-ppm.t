#!/bin/env perl 

BEGIN {
  unless(grep /blib/, @INC) {
    chdir 't' if -d 't';
    unshift @INC, '../lib' if -d '../lib';
  }
}

#use strict;
BEGIN {
print "1..0 # Skip: ActiveState's ppmserver.pl server seems to be broken - skipping tests\n";
exit 0;
};

use Test;
use SOAP::Lite 
  on_fault => sub {
    my $soap = shift;
    my $res = shift;
    ref $res ? warn(join " ", "SOAP FAULT:", $res->faultstring, "\n") 
             : warn(join " ", "TRANSPORT ERROR:", $soap->transport->status, "\n");
    return new SOAP::SOM;
  }
;

my($a, $s, $r);

my $proxy = 'http://ppm.activestate.com/cgibin/PPM/ppmserver.pl';

# ------------------------------------------------------
use SOAP::Test;

$s = SOAP::Lite->uri('urn:/PPMServer')->proxy($proxy)->on_fault(sub{});
eval { $s->transport->timeout($SOAP::Test::TIMEOUT = $SOAP::Test::TIMEOUT) };
$r = $s->test_connection;

unless (defined $r && defined $r->envelope) {
  print "1..0 # Skip: ", $s->transport->status, "\n"; 
  exit;
}
# ------------------------------------------------------

plan tests => 3;

{
# ActiveState's PPM server (http://activestate.com/)
  print "ActiveState's PPM server test(s)...\n";
  $s = SOAP::Lite 
    -> uri('urn:/PPMServer')
    -> proxy($proxy)
  ;

  $r = $s->fetch_ppd('SOAP-Lite')->result;
# use Data::Dumper;
# print Dumper $r;

  ok($r =~ 'SOAP-Lite'); 
  ok($r =~ 'Paul Kulchenko'); 

  $r = $s->fetch_ppd('SOAP-Super-Lite')->result;

  ok(!defined $r); 
}
