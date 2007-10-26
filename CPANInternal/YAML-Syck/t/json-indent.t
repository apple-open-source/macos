use strict;
use t::TestYAML ();
use JSON::Syck;
use Test::More tests => 1;

my $str = "foo\nbar\nbaz\n";
my $json = JSON::Syck::Dump({ str => $str });

unlike $json, qr/^  bar/m;
