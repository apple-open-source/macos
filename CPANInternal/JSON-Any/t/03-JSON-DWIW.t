#!perl -T
use strict;
use Test::More;

eval "use JSON::Any qw(DWIW)";
if ($@) {
    plan skip_all => "JSON::DWIW not installed: $@";
}
else {
        plan tests => 2;
}

diag("Testing JSON::DWIW backend");
my ( $json, $js, $obj );

# encoding bare keys
ok( $json = JSON::Any->new( bare_keys => 1 ) );
$js = $json->to_json( { var1 => "val2" } );
is( $js, '{var1:"val2"}' );

