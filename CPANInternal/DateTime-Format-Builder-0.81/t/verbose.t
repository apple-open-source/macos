use strict;
use warnings;

use Test::More;

use DateTime::Format::Builder;

# Does verbose() work properly?
SKIP: {
    skip "This test requires perl 5.8", 4 unless $] >= 5.007;
    skip "Verbose is temporarily out of it", 4;

    my $str;
    undef $SampleClass1::fh;    # just to un-warn
    eval q{
    open $SampleClass1::fh, '>', \$str
        or die "Cannot open string for writing!";
    };

    eval q[
    package SampleClass1;
    use DateTime::Format::Builder
        verbose => $SampleClass1::fh,
        parsers => {
        parse_datetime => [
        [
            preprocess => sub { my %args = @_; $args{input} },
        ],
        {
            regex => qr/^(\d{4})(\d\d)(d\d)(\d\d)(\d\d)(\d\d)$/,
            params => [qw( year month day hour minute second )],
            on_fail => sub { my %args = @_; $args{input} },
        },
        {
            preprocess => sub { my %args = @_; $args{input} },
            postprocess => sub { my %args = @_; $args{input} },
            on_match => sub { my %args = @_; $args{input} },
            regex => qr/^(\d{4})(\d\d)(\d\d)$/,
            params => [qw( year month day )],
        },
        {
            length => 8,
            regex => qr/^abcdef$/,
            params => [qw( year month day )],
        }
        ],
        };
    ];
    ok( !$@, "No errors when creating the class." );

    diag $@ if $@;

    my $parser = SampleClass1->new();
    isa_ok( $parser => 'SampleClass1' );

    my $input = "20040506";
    my $dt = eval { $parser->parse_datetime($input) };
    isa_ok( $dt => 'DateTime' );

    # Should have some data awaiting us now.
    close $SampleClass1::fh;
    like( $str, qr/$input/, "Logging data contains input." );
}

done_testing();
