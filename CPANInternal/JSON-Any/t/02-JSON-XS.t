#!perl -T

use utf8;
use strict;
use Test::More;

eval "use JSON::Any qw(XS)";
if ($@) {
    plan skip_all => "JSON::XS not installed: $@";
}
else {
    plan tests => 20;
}

skip "JSON::XS not installed: $@", 1 if $@;
diag("Testing JSON::XS backend");

is( JSON::Any->encode({foo=>'bar'}), qq[{"foo":"bar"}] );

my ( $json, $js, $obj );
ok( $json = JSON::Any->new( allow_nonref => 1, utf8 => 1 ) );
is( $json->encode("Ã¼"), qq["Ã¼"] );

foreach my $bytes ( 0, 1 ) {
    foreach my $utf8_support ( 0, 1 ) {
        my $str = "Ã¼";
        utf8::encode($str) if $bytes;
        ok( $json = JSON::Any->new( allow_nonref => 1, utf8 => $utf8_support ) );

        if ( $utf8_support ) { # only then do we make assumptions about the output json
            is( $json->encode($str), qq["$str"] );
        }

        is( $json->decode($json->encode($str)), $str, "round tripping" );
    }
}

ok( $json = JSON::Any->new( allow_nonref => 1, ascii => 1, utf8 => 1 ) );
is( $json->encode( chr 0x8000 ), '"\u8000"' );
ok(
    $json = JSON::Any->new(
        allow_nonref => 1,
        ascii        => 1,
        utf8         => 1,
        pretty       => 1
    )
);
is( $json->encode( chr 0x10402 ), '"\ud801\udc02"' );
ok( $json = JSON::Any->new( allow_nonref => 1, utf8 => 1 ) );

is( $json->encode("Ã¼"), qq["\xc3\xbc\"] );

is( JSON::Any->encode({foo=>'bar'}), qq[{"foo":"bar"}] );

