#!/usr/bin/perl -w

use strict;

use JSON::RPC::Client;


my $cgi_uri = 'http://example.com/cgi-bin/json/server.cgi/API'; # CGI
my $uri     = 'http://example.com/jsonrpc/API'; # Apache2 or daemon


my $client = new JSON::RPC::Client;

$client->prepare( $uri, ['sum', 'echo'] ); # if call /API/Sublcass, sum method return (sum * 2)

print $client->sum(10, 23), "\n";
print $client->echo("abc\ndef"), "\n";



my $callobj = {
   method  => 'sum2',
   params  => [ 17, 25 ], # ex.) params => { a => 20, b => 10 } for JSON-RPC v1.1
};

my $res = $client->call($cgi_uri, $callobj);

if($res) {
   if ($res->is_error) {
       print "Error : ", $res->error_message;
   }
   else {
       print $res->result;
   }
}
else {
   print $client->status_line;
}




   