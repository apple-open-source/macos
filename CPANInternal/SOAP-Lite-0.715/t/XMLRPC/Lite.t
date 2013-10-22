use strict;
use warnings;
use Test::More tests => 2; #qw(no_plan);

use_ok qw(XMLRPC::Lite);

my $serializer = XMLRPC::Serializer->new();

unlike $serializer->envelope('response', 'foo', 'bar'), qr{foo}, 'method name stripped off';