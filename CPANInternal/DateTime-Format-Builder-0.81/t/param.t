use strict;
use warnings;

use Test::More;

use DateTime::Format::Builder;

my $sample = 'SampleClassWithArgs1';

{
    my $parser = DateTime::Format::Builder->parser(
        {
            params      => [qw( year month day hour minute second )],
            regex       => qr/^(\d\d\d\d)(\d\d)(\d\d)T(\d\d)(\d\d)(\d\d)$/,
            postprocess => sub {
                my %p = (@_);
                $p{parsed}->{time_zone} = $p{args}->[0];
                1;
                }
        }
    );

    my $dt = $parser->parse_datetime( "20030716T163245", 'Europe/Berlin' );
    is( $dt->time_zone->name, 'Europe/Berlin' );
}

{
    DateTime::Format::Builder->create_class(
        class   => $sample,
        parsers => {
            parse_datetime => [
                [
                    preprocess => sub {
                        my %p = (@_);
                        $p{parsed}->{time_zone} = $p{args}->[0];
                        return $p{input};
                    },
                ],
                {
                    params => [qw( year month day hour minute second )],
                    regex  => qr/^(\d\d\d\d)(\d\d)(\d\d)T(\d\d)(\d\d)(\d\d)$/,
                }
            ],
        },
    );

    my $dt = $sample->parse_datetime( "20030716T163245", 'Asia/Singapore' );
    is( $dt->time_zone->name, 'Asia/Singapore' );
}

{
    $sample++;
    DateTime::Format::Builder->create_class(
        class   => $sample,
        parsers => {
            parse_datetime => [
                [
                    preprocess => sub {
                        my %p = @_;
                        my %o = @{ $p{args} };
                        $p{parsed}->{time_zone} = $o{global} if $o{global};
                        return $p{input};
                    },
                ],
                {
                    params => [qw( year month day hour minute second )],
                    regex  => qr/^(\d\d\d\d)(\d\d)(\d\d)T(\d\d)(\d\d)(\d\d)$/,
                    preprocess => sub {
                        my %p = @_;
                        my %o = @{ $p{args} };
                        $p{parsed}->{time_zone} = $o{pre} if $o{pre};
                        return $p{input};
                    },
                    postprocess => sub {
                        my %p = @_;
                        my %o = @{ $p{args} };
                        $p{parsed}->{time_zone} = $o{post} if $o{post};
                        return 1;
                    },
                },
            ],
        }
    );

    my %tests = (
        global => 'Africa/Cairo',
        pre    => 'Europe/London',
        post   => 'Australia/Sydney',
    );

    while ( my ( $callback, $value ) = each %tests ) {
        my $parser = $sample->new();
        my $dt     = $parser->parse_datetime(
            "20030716T163245",
            $callback => $value,
        );
        is( $dt->time_zone->name, $value );
    }
}

done_testing();
