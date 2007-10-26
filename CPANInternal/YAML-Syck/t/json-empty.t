use strict;
use t::TestYAML ();
use Test::More tests => 1;
use JSON::Syck;

my $data = JSON::Syck::Load(q({"i":{"cid":"123","sid":"123","cd":"","v":"2.0","m":"h01iSTI5"},"r":{"adbox1":{"w":320,"h":200},"adbox2":{"w":320,"h":200},"adbox3":{"w":320,"h":200},"adbox1_info":{"w":320,"h":32},"adbox2_info":{"w":320,"h":32},"adbox3_info":{"w":320,"h":32},"adbox4":{"w":320,"h":200},"adbox5":{"w":320,"h":200},"adbox6":{"w":320,"h":200},"adbox4_info":{"w":320,"h":32},"adbox5_info":{"w":320,"h":32},"adbox6_info":{"w":320,"h":32},"adbox4_pick":{"w":320,"h":32},"adbox5_pick":{"w":320,"h":32},"adbox6_rate":{"w":320,"h":32}}}));

ok keys %$data;
