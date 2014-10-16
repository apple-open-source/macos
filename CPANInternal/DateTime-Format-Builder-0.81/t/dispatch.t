use strict;
use warnings;

use Test::More;

use DateTime::Format::Builder;

{
    eval q[
    package SampleDispatch;
    use DateTime::Format::Builder
    (
        parsers => {
        parse_datetime => [
            {
            Dispatch => sub {
                return 'fnerk';
            }
            }
        ]
        },
        groups => {
        fnerk => [
            {
            regex => qr/^(\d{4})(\d\d)(\d\d)$/,
            params => [qw( year month day )],
            },
        ]
        }
    );
    ];
    ok( !$@, "No errors when creating the class." );
    if ($@) { diag $@; exit }

    my $parser = SampleDispatch->new();
    isa_ok( $parser => 'SampleDispatch' );

    my $dt = eval { $parser->parse_datetime("20040506") };
    ok( !$@, "No errors when parsing." );
    if ($@) { diag $@; exit }
    isa_ok( $dt => 'DateTime' );

    is( $dt->year  => 2004, 'Year is 2004' );
    is( $dt->month => 5,    'Month is 5' );
    is( $dt->day   => 6,    'Day is 6' );

    eval { $parser->fnerk };
    ok( $@, "There is no fnerk." );

}

{
    eval q[
        package SampleDispatchB;

        use DateTime::Format::Builder;

        DateTime::Format::Builder->create_class(
            parsers => {
                parse_datetime => [
                    {
                        Dispatch => sub {
                            return( 8, 6 );
                        }
                    },
                ],
            },
            groups => {
                8 => [
                    {
                        regex  => qr/^ (\d{4}) (\d\d) (\d\d) $/x,
                        params => [ qw( year month day ) ],
                    },
                ],
                6 => [
                    {
                        regex  => qr/^ (\d{4}) (\d\d) $/x,
                        params => [ qw( year month ) ],
                    },
                ],
            }
        );
    ];

    ok( !$@, "No errors when creating the class." );
    if ($@) { diag $@; exit }

    my $parser = SampleDispatchB->new();
    isa_ok( $parser => 'SampleDispatchB' );

    {
        my $dt = eval { $parser->parse_datetime("20040506") };
        ok( !$@, "No errors when parsing." );
        if ($@) { diag $@; exit }
        isa_ok( $dt => 'DateTime' );

        is( $dt->year  => 2004, 'Year is 2004' );
        is( $dt->month => 5,    'Month is 5' );
        is( $dt->day   => 6,    'Day is 6' );
    }

    {
        my $dt = eval { $parser->parse_datetime("200311") };
        ok( !$@, "No errors when parsing." );
        if ($@) { diag $@; exit }
        isa_ok( $dt => 'DateTime' );

        is( $dt->year  => 2003, 'Year is 2003' );
        is( $dt->month => 11,   'Month is 11' );
        is( $dt->day   => 1,    'Day is 1' );
    }

    eval { $parser->fnerk };
    ok( $@, "There is no fnerk." );

}

# ------------------------------------------------------------------------

pass "All done.";

done_testing();
