use strict;
use warnings;
use Test::More tests => 6;

use Data::Dump qw/dump/;

if ("abc" =~ /(.+)/) {
    is(dump($1), '"abc"');
    is(dump(\$1), '\"abc"');
    is(dump([$1]), '["abc"]');
}

if ("123" =~ /(.+)/) {
    is(dump($1), "123");
    is(dump(\$1), '\123');
    is(dump([$1]), '[123]');
}
