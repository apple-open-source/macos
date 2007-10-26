use t::TestYAML tests => 3;

ok(YAML::Syck->VERSION);
is(Dump("Hello, world"), "--- Hello, world\n");
is(Load("--- Hello, world\n"), "Hello, world");
