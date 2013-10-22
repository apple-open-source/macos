use warnings;
use strict;

use Test::More tests => 2;

require_ok "hostname.pl";

my $host = eval { &hostname };
if($@) {
	like $@, qr/Cannot get host name/;
} else {
	ok 1;
}

1;
