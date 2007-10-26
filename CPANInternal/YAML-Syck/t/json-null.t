use strict;
use t::TestYAML ();
use Test::More tests => 2;

use JSON::Syck;

my $dat = JSON::Syck::Dump({ foo => undef });
like $dat, qr/null/;

$dat = JSON::Syck::Dump({ foo => !1 });
unlike $dat, qr/"foo":}/;




