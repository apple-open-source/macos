#!perl -T
use strict;
use Test::More;

eval "use JSON::Any qw(DWIW)";
if ($@) {
    plan skip_all => "JSON::DWIW not installed: $@";
}
else {
        plan tests => 6;
}

diag("Testing JSON::DWIW backend");
my ( $json, $js, $obj );

# encoding bare keys
ok( $json = JSON::Any->new( bare_keys => 1 ) );
$js = $json->to_json( { var1 => "val2" } );
is( $js, '{var1:"val2"}' );

# testing the truth
$obj = { foo => JSON::Any->true };
$js = JSON::Any->objToJson($obj);
is($js,'{"foo":true}');

$obj = { foo => JSON::Any->false };
$js = JSON::Any->objToJson($obj);
is($js,'{"foo":false}');

$obj = { foo => $json->true };
$js = $json->objToJson($obj);
is($js,'{foo:true}');

$obj = { foo => $json->false };
$js = $json->objToJson($obj);
is($js,'{foo:false}');
