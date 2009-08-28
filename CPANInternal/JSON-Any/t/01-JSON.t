#!perl
$|++;
use strict;
use Test::More;
eval "use JSON::Any qw(JSON)";
if ($@) {
    plan skip_all => "JSON.pm not installed: $@";
}
else {
    plan tests => 15;
}

diag("Testing JSON.pm backend");
my ( $js, $obj );

ok(my $json_obj = JSON::Any->new());
isa_ok($json_obj, 'JSON::Any');
isa_ok($json_obj->handler, 'JSON');

$js  = q|{}|;
$obj = $json_obj->jsonToObj($js);
$js  = $json_obj->objToJson($obj);
is($js,'{}');

$js  = q|[]|;
$obj = $json_obj->jsonToObj($js);
$js  = $json_obj->objToJson($obj);
is($js,'[]');

$js  = q|{"foo":"bar"}|;
$obj = $json_obj->jsonToObj($js);
is($obj->{foo},'bar');
$js = $json_obj->objToJson($obj);
is($js,'{"foo":"bar"}');

$js  = q|{"foo":""}|;
$obj = $json_obj->jsonToObj($js);
$js = $json_obj->objToJson($obj);
is($js,'{"foo":""}');

$js  = q|{"foo":" "}|;
$obj = $json_obj->jsonToObj($js);
$js = $json_obj->objToJson($obj);
is($js,'{"foo":" "}');


$js  = q|{}|;
$obj = JSON::Any->jsonToObj($js);
$js  = JSON::Any->objToJson($obj);
is($js,'{}');

$js  = q|[]|;
$obj = JSON::Any->jsonToObj($js);
$js  = JSON::Any->objToJson($obj);
is($js,'[]');

$js  = q|{"foo":"bar"}|;
$obj = JSON::Any->jsonToObj($js);
is($obj->{foo},'bar');
$js = JSON::Any->objToJson($obj);
is($js,'{"foo":"bar"}');

$js  = q|{"foo":""}|;
$obj = JSON::Any->jsonToObj($js);
$js = JSON::Any->objToJson($obj);
is($js,'{"foo":""}');

$js  = q|{"foo":" "}|;
$obj = JSON::Any->jsonToObj($js);
$js = JSON::Any->objToJson($obj);
is($js,'{"foo":" "}');