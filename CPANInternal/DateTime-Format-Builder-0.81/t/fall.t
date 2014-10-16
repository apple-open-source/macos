use strict;
use warnings;

use Test::More;

use DateTime::Format::Builder;

SKIP: {
    my @three = map { "DateTime::Format::$_" } qw( HTTP Mail IBeat );
    my @fails;
    for my $mod (@three) {
        eval "require $mod";
        push @fails, $mod if $@;
    }
    skip "@fails not installed.", 3 if @fails;

    eval qq|package DateTime::Format::Fall;|
        . join( "", map { "use $_;\n" } @three ) . q|
        use DateTime::Format::Builder (
        parsers => { parse_datetime => [
        |
        . join(
        "",
        map { qq|sub { eval { $_->parse_datetime( \$_[1] ) } },\n| } @three
        )
        . q|
        ]});

        1;
    |;

    die $@ if $@;

    my $get = sub {
        eval {
            DateTime::Format::Fall->parse_datetime( $_[0] )
                ->set_time_zone('UTC')->datetime;
        };
    };

    for ( '@d19.07.03 @704', '20030719T155345Z' ) {
        my $dt = $get->($_);
        is $dt, "2003-07-19T15:53:45", "Can parse [$_]";
    }

    for ('gibberish') {
        my $dt = $get->($_);
        ok( !defined $dt, "Shouldn't parse [$_]" );
    }
}

done_testing();
