#!perl -w

# t/008_epoch.t - Epoch (%s) tests

use Test::More 0.88;
use DateTime;
use DateTime::Format::Strptime;

my $time = time;

# Epoch in, epoch out, now.
test(
    pattern   => "%s",
    time_zone => 'Asia/Manila',
    locale    => 'en_PH',
    input     => $time,
    epoch     => $time,
);

# is UTC recognized?
test(
    pattern   => "%a %d %b %Y %H:%M:%S %p %Z",
    time_zone => 'UTC',
    locale    => 'en_US',
    input     => "Thu 08 Jul 2010 09:49:02 AM UTC",
    epoch     => 1278582542,
);

# diag("Epoch with a no given time_zone assumes 'floating'. (Though when given an epoch, really should assume UTC ..)");
{
    my $parser = DateTime::Format::Strptime->new(
        pattern  => '%s',
        locale   => 'en',
        on_error => 'undef',
    );
    isa_ok( $parser, 'DateTime::Format::Strptime' );
    my $parsed = $parser->parse_datetime('1235282552');
    isa_ok( $parsed, 'DateTime' );
    is( $parsed->year,            2009 );
    is( $parsed->month,           2 );
    is( $parsed->day,             22 );
    is( $parsed->hour,            6 );
    is( $parsed->minute,          2 );
    is( $parsed->second,          32 );
    is( $parsed->nanosecond * 1,  0 );
    is( $parsed->time_zone->name, 'floating' );
}

# diag("Epoch with a time_zone should return the correct time for that TZ when the epoch occurs in UTC");
{
    my $parser = DateTime::Format::Strptime->new(
        pattern   => '%s',
        locale    => 'en',
        on_error  => 'undef',
        time_zone => 'Asia/Manila',
    );
    isa_ok( $parser, 'DateTime::Format::Strptime' );
    my $parsed = $parser->parse_datetime('1235282552');
    isa_ok( $parsed, 'DateTime' );
    is( $parsed->year,            2009 );
    is( $parsed->month,           2 );
    is( $parsed->day,             22 );
    is( $parsed->hour,            14 );
    is( $parsed->minute,          2 );
    is( $parsed->second,          32 );
    is( $parsed->nanosecond * 1,  0 );
    is( $parsed->time_zone->name, 'Asia/Manila' );
}

sub test {
    my %arg = @_;

    my $strptime = DateTime::Format::Strptime->new(
        pattern    => $arg{pattern}    || '%F %T',
        locale     => $arg{locale}     || 'en',
        time_zone  => $arg{time_zone}  || 'UTC',
        diagnostic => $arg{diagnostic} || 0,
        on_error   => 'undef',
    );
    isa_ok( $strptime, 'DateTime::Format::Strptime' );

    my $parsed = $strptime->parse_datetime( $arg{input} );
    isa_ok( $parsed, 'DateTime' );

    is( $parsed->epoch, $arg{epoch} );
}

done_testing();
