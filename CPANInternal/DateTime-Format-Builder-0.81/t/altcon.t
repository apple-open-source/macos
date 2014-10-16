use strict;
use warnings;

use Test::More;

use DateTime::Format::Builder;

# ------------------------------------------------------------------------

sub do_check {
    my ( $parser, $date, $values ) = @_;
    my $parsed = $parser->parse( 'DateTime::Format::Builder', $date );
    isa_ok( $parsed => 'DateTime' );
    is( $parsed->year()  => $values->[0], "Year is right" );
    is( $parsed->month() => $values->[1], "Month is right" );
    is( $parsed->day()   => $values->[2], "Day is right" );
}

{
    my $parser = DateTime::Format::Builder->create_parser(
        {
            #YYYY-DDD 1985-102
            regex       => qr/^ (\d{4}) -?? (\d{3}) $/x,
            params      => [qw( year day_of_year )],
            constructor => [ 'DateTime', 'from_day_of_year' ],
        },
        {
            regex       => qr/^ (\d{4}) foo (\d{3}) $/x,
            params      => [qw( year day_of_year )],
            constructor => sub {
                my $self = shift;
                DateTime->from_day_of_year(@_);
            },
        }
    );

    my %dates = (
        '1985-102' => [ 1985, 4, 12 ],
        '2004-102' => [ 2004, 4, 11 ],    # leap year
    );

    for my $date ( sort keys %dates ) {
        my $values = $dates{$date};
        do_check( $parser, $date, $values );
        $date =~ s/-/foo/;
        do_check( $parser, $date, $values );
    }
}

{
    my $parser = DateTime::Format::Builder->create_parser(
        {
            regex       => qr/^ (\d+) $/x,
            params      => [qw( epoch )],
            constructor => [ 'DateTime', 'from_epoch' ]
        }
    );
    my %epochs = (
        1057279398 => '2003-07-04T00:43:18',
    );
    for my $epoch ( sort keys %epochs ) {
        my $check = $epochs{$epoch};
        my $dt = $parser->parse( 'DateTime::Format::Builder', $epoch );
        isa_ok( $dt => 'DateTime' );
        is( $dt->datetime => $check, "Epoch of $epoch to $check" );
    }
}

pass 'All done';

done_testing();
