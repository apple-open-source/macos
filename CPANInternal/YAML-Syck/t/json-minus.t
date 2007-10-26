use strict;
use t::TestYAML ();
use JSON::Syck;
use Test::More tests => 1;

my $data = JSON::Syck::Load('{"i":-2}');

is $data->{i}, -2;
