use strict;

use Test::More tests => 10765;

use DateTime::Format::ISO8601;

{
    is( DateTime::Format::ISO8601->DefaultCutOffYear, 49 );
    my $iso_parser = DateTime::Format::ISO8601->new;
    is( $iso_parser->cut_off_year, 49 );
}

foreach ( 0 .. 99 ) {
    DateTime::Format::ISO8601->DefaultCutOffYear( $_ );
    is( DateTime::Format::ISO8601->DefaultCutOffYear, $_ );
    my $iso_parser = DateTime::Format::ISO8601->new;
    is( $iso_parser->cut_off_year, $_ );
}

foreach ( -10 .. -1, 100 .. 110 ) {
    eval { DateTime::Format::ISO8601->DefaultCutOffYear( $_ ) };
    like( $@, qr/did not pass the 'is between 0 and 99' callback/ );
}

# restore default cut off year behavior
DateTime::Format::ISO8601->DefaultCutOffYear( 49 );

foreach ( 0 .. 99 ) {
    {
        my $iso_parser = DateTime::Format::ISO8601->new( cut_off_year => $_ );
        isa_ok( $iso_parser, 'DateTime::Format::ISO8601' );
        is( $iso_parser->cut_off_year, $_ );
    }

    {
        my $iso_parser = DateTime::Format::ISO8601->new->set_cut_off_year( $_ );
        is( $iso_parser->cut_off_year, $_ );
    }
}

foreach ( -10 .. -1, 100 .. 110 ) {
    eval { DateTime::Format::ISO8601->new( cut_off_year => $_ ) };
    like( $@, qr/did not pass the 'is between 0 and 99' callback/ );

    eval { DateTime::Format::ISO8601->new->set_cut_off_year( $_ ) };
    like( $@, qr/did not pass the 'is between 0 and 99' callback/ );
}

{
    foreach ( 0 .. 49 ) {
        my $dt = DateTime::Format::ISO8601->parse_datetime( "-" . sprintf( "%02d", $_ ) );
        is( $dt->year, "20" . sprintf( "%02d", $_ ) );
    }
    foreach ( 50 .. 99 ) {
        my $dt = DateTime::Format::ISO8601->parse_datetime( "-$_" );
        is( $dt->year, "19$_" );
    }
}

{
    my $iso_parser = DateTime::Format::ISO8601->new;

    foreach ( 0 .. 49 ) {
        my $dt = $iso_parser->parse_datetime( "-" . sprintf( "%02d", $_ ) );
        is( $dt->year, "20" . sprintf( "%02d", $_ ) );
    }
    foreach ( 50 .. 99 ) {
        my $dt = $iso_parser->parse_datetime( "-$_" );
        is( $dt->year, "19$_" );
    }
}

foreach ( 0 .. 99 ) {
    my $iso_parser = DateTime::Format::ISO8601->new( cut_off_year => $_ );

    foreach ( 0 .. $iso_parser->cut_off_year ) {
        my $dt = $iso_parser->parse_datetime( "-" . sprintf( "%02d", $_ ) );
        is( $dt->year, "20" . sprintf( "%02d", $_ ) );
    }
    foreach ( ( $iso_parser->cut_off_year + 1 ) .. 99 ) {
        my $dt = $iso_parser->parse_datetime( "-" . sprintf( "%02d", $_ ) );
        is( $dt->year, "19" . sprintf( "%02d", $_ ) );
    }
}
