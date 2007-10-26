use strict;

use Test::More tests => 1074;

use DateTime::Format::ISO8601;

{
    is( DateTime::Format::ISO8601->DefaultLegacyYear, 1 );
    my $iso_parser = DateTime::Format::ISO8601->new;
    is( $iso_parser->legacy_year, 1 );
}

foreach ( 0, 1, undef ) {
    DateTime::Format::ISO8601->DefaultLegacyYear( $_ );
    is( DateTime::Format::ISO8601->DefaultLegacyYear, $_ );
    my $iso_parser = DateTime::Format::ISO8601->new;
    is( $iso_parser->legacy_year, $_ );
}

foreach ( -10 .. -1, 2 .. 10 ) {
    eval { DateTime::Format::ISO8601->DefaultLegacyYear( $_ ) };
    like( $@, qr/did not pass the 'is 0, 1, or undef' callback/ );
}

# restore default legacy year behavior
DateTime::Format::ISO8601->DefaultLegacyYear( 1 );

foreach ( 0, 1, undef ) {
    my $iso_parser = DateTime::Format::ISO8601->new( legacy_year => $_ );
    isa_ok( $iso_parser, 'DateTime::Format::ISO8601' );
    is( $iso_parser->legacy_year, $_ );

    {
        my $iso_parser = DateTime::Format::ISO8601->new->set_legacy_year( $_ );
        is( $iso_parser->legacy_year, $_ );
    }
}

foreach ( -10 .. -1, 2 .. 10 ) {
    eval { DateTime::Format::ISO8601->new( legacy_year => $_ ) };
    like( $@, qr/did not pass the 'is 0, 1, or undef' callback/ );

    eval { DateTime::Format::ISO8601->new->set_legacy_year( $_ ) };
    like( $@, qr/did not pass the 'is 0, 1, or undef' callback/ );
}

foreach ( 0 .. 99 ) {
    my $iso_parser = DateTime::Format::ISO8601->new(
        legacy_year     => 0,
        base_datetime   => DateTime->new( year => $_ * 100 ),
    );

    foreach ( 0 .. 9 ) {
        $_ *= 10;
        my $dt = $iso_parser->parse_datetime( "-" . sprintf( "%02d", $_ ) );
        is( $dt->year, sprintf( "%d", $iso_parser->base_datetime->strftime( "%C" )
            . sprintf( "%02d", $_ ) )
        );
    }
}
