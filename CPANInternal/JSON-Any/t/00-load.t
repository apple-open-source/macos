#!perl -T

use Test::More;

BEGIN {

    # Count who's installed
    my @order = qw(JSON::XS JSON JSON::DWIW JSON::Syck);
    my $count = scalar grep { eval "require $_"; not $@; } @order;

    unless ($count) {    # need at least one
        plan skip_all => "Can't find a JSON package.";
        exit;
    }

    # if we're here we have *something* that will work
    plan tests => 7;
    use_ok('JSON::Any');
}

diag("Testing JSON::Any $JSON::Any::VERSION, Perl $], $^X");
can_ok( JSON::Any, qw(new) );
can_ok( JSON::Any, qw(objToJson jsonToObj) );
can_ok( JSON::Any, qw(to_json from_json ) );
can_ok( JSON::Any, qw(Dump Load ) );
can_ok( JSON::Any, qw(encode decode ) );

is( JSON::Any->objToJson( { foo => 'bar' } ), q[{"foo":"bar"}] );
