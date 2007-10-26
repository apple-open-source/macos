print "1..1\n";

use strict;
use LWP::UserAgent;

my $ua = LWP::UserAgent->new(keep_alive => 1);

my $res = $ua->get(
  "http://jigsaw.w3.org/HTTP/neg",
    Connection => "close",
);

print $res->as_string, "\n";

print "not " unless $res->code == 300;
print "ok 1\n";
