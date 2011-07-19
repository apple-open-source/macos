#!/usr/bin/perl -w

use strict;

use Test::More tests => 38;

use DateTime;

{
    # Tests creating objects from epoch time
    my $t1 = DateTime->from_epoch( epoch => 0 );
    is( $t1->epoch, 0, "epoch should be 0" );

    is( $t1->second, 0, "seconds are correct on epoch 0" );
    is( $t1->minute, 0, "minutes are correct on epoch 0" );
    is( $t1->hour, 0, "hours are correct on epoch 0" );
    is( $t1->day, 1, "days are correct on epoch 0" );
    is( $t1->month, 1, "months are correct on epoch 0" );
    is( $t1->year, 1970, "year is correct on epoch 0" );
}

{
    my $dt = DateTime->from_epoch( epoch => '3600' );
    is( $dt->epoch, 3600, 'creation test from epoch = 3600 (compare to epoch)');
}

{
    # these tests could break if the time changed during the next three lines
    my $now = time;
    my $nowtest = DateTime->now();
    my $nowtest2 = DateTime->from_epoch( epoch => $now );
    is( $nowtest->hour, $nowtest2->hour, "Hour: Create without args" );
    is( $nowtest->month, $nowtest2->month, "Month : Create without args" );
    is( $nowtest->minute, $nowtest2->minute, "Minute: Create without args" );
}

{
    my $epochtest = DateTime->from_epoch( epoch => '997121000' );

    is( $epochtest->epoch, 997121000,
        "epoch method returns correct value");
    is( $epochtest->hour, 18, "hour" );
    is( $epochtest->min, 3, "minute" );
}

{
    my $dt = DateTime->from_epoch( epoch => 3600 );
    $dt->set_time_zone('+0100');

    is( $dt->epoch, 3600, 'epoch is 3600' );
    is( $dt->hour, 2, 'hour is 2' );
}

{

    my $dt = DateTime->new( year  => 1970,
                            month => 1,
                            day   => 1,
                            hour  => 0,
                            time_zone => '-0100',
                          );

    is( $dt->epoch, 3600, 'epoch is 3600' );
}

{

    my $dt = DateTime->from_epoch( epoch => 0,
                                   time_zone => '-0100',
                                 );

    is( $dt->offset, -3600, 'offset should be -3600' );
    is( $dt->epoch, 0, 'epoch is 0' );
}

# Adding/subtracting should affect epoch
{
    my $expected = 1049160602;
    my $epochtest = DateTime->from_epoch( epoch => $expected  );

    is( $epochtest->epoch, $expected,
        "epoch method returns correct value ($expected)");
    is( $epochtest->hour, 1, "hour" );
    is( $epochtest->min, 30, "minute" );

    $epochtest->add( hours => 2 );
    $expected += 2 * 60 * 60;

    is( $epochtest->hour, 3, "adjusted hour" );
    is( $epochtest->epoch, $expected,
        "epoch method returns correct adjusted value ($expected)");

}

{
    my $dt = DateTime->from_epoch( epoch => 0.5 );
    is( $dt->nanosecond, 500_000_000, 'nanosecond should be 500,000,000 with 0.5 as epoch' );

    is( $dt->epoch, 0, 'epoch should be 0' );
    is( $dt->hires_epoch, 0.5, 'hires_epoch should be 0.5' );
}

{
    my $dt = DateTime->from_epoch( epoch => 0.1234567891 );
    is( $dt->nanosecond, 123_456_789, 'nanosecond should be an integer ' );
}

my $negative_epoch_ok = defined( (localtime(-1))[0] ) ? 1 : 0;

SKIP:
{
    skip 'Negative epoch times do not work on some operating systems, including Win32', 4
        unless $negative_epoch_ok;

    is( DateTime->new( year => 1904 )->epoch, -2082844800,
        "epoch should work back to at least 1904" );

    my $dt = DateTime->from_epoch( epoch => -2082844800 );
    is( $dt->year, 1904, 'year should be 1904' );
    is( $dt->month,   1, 'month should be 1904' );
    is( $dt->day,     1, 'day should be 1904' );
}

{
    package Number::Overloaded;
    use overload
        "0+"          => sub { $_[0]->{num} },
        fallback      => 1;

    sub new { bless { num => $_[1] }, $_[0] }
}

{
    my $time = Number::Overloaded->new(12345);

    my $dt = DateTime->from_epoch( epoch => $time );
    is( $dt->epoch, 12345, 'can pass overloaded object to from_epoch' );

    $time = Number::Overloaded->new(12345.1234);
    $dt = DateTime->from_epoch( epoch => $time );
    is( $dt->epoch, 12345, 'decimal epoch in overloaded object' );
}

SKIP:
{
    skip 'Negative epoch times do not work on some operating systems, including Win32', 1
        unless $negative_epoch_ok;

    my $time = Number::Overloaded->new(-12345);
    my $dt = DateTime->from_epoch( epoch => $time );

    is( $dt->epoch, -12345, 'negative epoch in overloaded object' );
}

{
    my @tests = ( 'asldkjlkjd',
                  '1234 foo',
                  'adlkj 1234',
                );

    for my $test (@tests)
    {
        eval
        {
            DateTime->from_epoch( epoch => $test );
        };

        like( $@, qr/did not pass regex check/,
              qq{'$test' is not a valid epoch value} );
    }
}
