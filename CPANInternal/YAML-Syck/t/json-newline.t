use JSON::Syck;
use Test::More tests => 1;

my $dump = JSON::Syck::Dump({a => "hello\nworld\n"});
is $dump, q({"a":"hello\nworld\n"});




