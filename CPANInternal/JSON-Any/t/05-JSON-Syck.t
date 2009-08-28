#!/usr/bin/perl -w

use strict;
use Test::More;
eval "use JSON::Any qw(Syck)";
if ($@) {
    plan skip_all => "JSON::Syck not installed: $@";
}
else {
    plan tests => 2;
}

ok( JSON::Any->new->objToJson( { foo => 1 } ) );
ok( JSON::Any->new->jsonToObj('{ "foo" : 1 }') );
