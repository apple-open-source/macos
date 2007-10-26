print "1..2\n";

use strict;
use LWP::UserAgent;

my $ua = LWP::UserAgent->new(keep_alive => 1);

my $res = $ua->get(
  "http://jigsaw.w3.org/HTTP/h-content-md5.html",
    "TE" => "deflate",
);

use Digest::MD5 qw(md5_base64);
print "not " unless $res->header("Content-MD5") eq md5_base64($res->content) . "==";
print "ok 1\n";

print $res->as_string;

my $etag = $res->header("etag");

$res = $ua->get(
  "http://jigsaw.w3.org/HTTP/h-content-md5.html",
    "TE" => "deflate",
    "If-None-Match" => $etag,
);
print $res->as_string;

print "not " unless $res->code eq "304" && $res->header("Client-Response-Num") == 2;
print "ok 2\n";
