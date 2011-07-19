#!perl -w

# t/009_regexp.t - Patterns as regular expressions

use Test::More tests => 21;
use DateTime;
use DateTime::Format::Strptime;

test(
    pattern => qr/%Y-%m-%d/,
    input   => '2009-07-13',
    output  => { year => 2009, month => 7, day => 13 }
);

test(
    pattern => qr/%Y-%m-%d Static Text/,
    input   => '2009-07-13 Static Text',
    output  => { year => 2009, month => 7, day => 13 }
);

test(
    pattern => qr/%Y-%m-%d \w+\s\w+/,
    input   => '2009-07-13 Static Text',
    output  => { year => 2009, month => 7, day => 13 }
);

test(
    pattern => qr/^%Y-%m-%d \w+\s\w+$/,
    input   => '2009-07-13 Static Text',
    output  => { year => 2009, month => 7, day => 13 }
);

eval {
    my $strptime = DateTime::Format::Strptime->new(
        pattern  => qr/^%Y-%m-%d \s+$/,
        on_error => 'croak',
    );
    my $parsed = $strptime->parse_datetime('2009-07-13 Static Text');
};
is( substr( $@, 0, 42 ), "Your datetime does not match your pattern.",
    "The strp pattern is OK, but the regex doesn't match the input." );

sub test {
    my %arg = @_;

    my $strptime = DateTime::Format::Strptime->new(
        pattern    => $arg{pattern}    || '%F %T',
        locale     => $arg{locale}     || 'en',
        time_zone  => $arg{time_zone}  || 'UTC',
        diagnostic => $arg{diagnostic} || 0,
        on_error   => $arg{on_error}   || 'undef',
    );
    isa_ok( $strptime, 'DateTime::Format::Strptime' );

    my $parsed = $strptime->parse_datetime( $arg{input} );
    isa_ok( $parsed, 'DateTime' );

    foreach my $k ( keys %{ $arg{output} } ) {
        is( $parsed->$k, $arg{output}{$k} );
    }
}
