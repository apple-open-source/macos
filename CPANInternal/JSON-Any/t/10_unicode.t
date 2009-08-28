#!perl
$|++;
use strict;
use utf8;
use Test::More;

eval "use JSON::Any";

if ($@) {
    plan skip_all => "$@";
    exit;
}

$ENV{JSON_ANY_CONFIG} = "utf8=1";

foreach my $backend qw(XS JSON DWIW Syck PC) {
    my $j = eval {
        JSON::Any->import($backend);
        JSON::Any->new;
    };

    diag "$backend: " . $@ and next if $@;

    $j and $j->handler or next;

    diag "handler is " . ( ref( $j->handler ) || $j->handlerType );

    plan 'no_plan' unless $ENV{JSON_ANY_RAN_TESTS};
    $ENV{JSON_ANY_RAN_TESTS} = 1;

    foreach my $text qw(foo שלום) {

        my $struct = [$text];

        my $frozen = $j->encode($struct);
        my $thawed = $j->decode($frozen);

        ok( utf8::is_utf8($frozen) || !scalar( $frozen !~ /[\w\d[:punct:]]/ ),
            "json output is utf8" );

        is_deeply( $thawed, $struct, "deeply" );

        is( $thawed->[0], $text, "text is the same" ) || eval {
            require Devel::StringInfo;
            my $d = Devel::StringInfo->new;
            $d->dump_info( $text, name => "expected" );
            $d->dump_info( $thawed->[0], name => "got" );
            $d->dump_info($frozen);
        };

        ok( utf8::is_utf8( $thawed->[0] ) || !scalar( $text !~ /[a-z]/ ),
            "text is utf8 if it needs to be" );

        if ( utf8::valid($frozen) ) {
            utf8::decode($frozen);

            my $thawed = $j->decode($frozen);

            is_deeply( $thawed, $struct, "deeply" );

            is( $thawed->[0], $text, "text is the same" ) || eval {
                require Devel::StringInfo;
                my $d = Devel::StringInfo->new;
                $d->dump_info( $text, name => "expected" );
                $d->dump_info( $thawed->[0], name => "got" );
                $d->dump_info($frozen);
            };

            ok( utf8::is_utf8( $thawed->[0] ) || !scalar( $text !~ /[a-z]/ ),
                "text is utf8 if it needs to be" );
        }
    }
}

plan skip_all => 'no JSON package with unicode support installed'
  unless $ENV{JSON_ANY_RAN_TESTS};
