#!perl
$!++;
use strict;
use Data::Dumper;
use Test::More;

eval "use JSON::Any";

if ($@) {
    plan skip_all => "$@";
    exit;
}

$Data::Dumper::Indent = 0;
$Data::Dumper::Terse  = 1;

# JSON::Syck doesn't support bools
my @backends = qw(XS JSON DWIW);

# make sure we test the JSON::PP backend instead of XS, twice
$ENV{PERL_JSON_BACKEND} = 0;

plan tests => @backends * 2 * 4;

test ($_) for @backends;

{
    package Boolean;
    sub true { 1 }
    sub false { '' }
}

sub test {
    my ($backend) = @_;

    SKIP: {
        my $j = eval {
            JSON::Any->import($backend);
            JSON::Any->new;
        };

        diag("$backend: " . $@), skip("Backend $backend failed to load", 8) if $@;

        $j and $j->handler or next;

        diag "handler is " . ( ref( $j->handler ) || $j->handlerType );

        for my $bool ( qw/true false/ ) {
            my $data = eval { JSON::Any->jsonToObj($bool) };
            ok ( !$@,  "inflated '$bool'" );
            ok ( eval { $data xor !Boolean->$bool }, "$bool evaluates to $bool" );

            $data = eval { JSON::Any->$bool };
            ok ( !$@, "JSON::Any->$bool returned a value" );
            ok ( eval { $data xor !Boolean->$bool }, "JSON::Any->$bool evaluates to $bool" );
        }
    };
}
