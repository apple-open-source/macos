#!perl
use strict;
my $r = shift;
$r->send_http_header('text/plain');
print "OK";
