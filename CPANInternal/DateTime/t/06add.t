#!/usr/bin/perl -w

use strict;

use Test::More tests => 537;

use DateTime;


my $t = DateTime->new( year => 1996, month => 11, day => 22,
                       hour => 18, minute => 30, second => 20,
                       time_zone => 'UTC',
                     );
$t->add( weeks => 8);

is( $t->year, 1997, "year rollover");
is( $t->month, 1, "month set on year rollover");
is( $t->datetime, '1997-01-17T18:30:20', 'okay on year rollover' );

$t->add( weeks => 2 );
is( $t->datetime, '1997-01-31T18:30:20', 'Adding weeks' );

$t->add( seconds => 15 );
is( $t->datetime, '1997-01-31T18:30:35', 'Adding seconds' );

$t->add( minutes => 12 );
is( $t->datetime, '1997-01-31T18:42:35', 'Adding minutes' );

$t->add( minutes => 25, hours => 3, seconds => 7 );
is( $t->datetime, '1997-01-31T22:07:42', 'Adding h,m,s' );

# Now, test the adding of durations
$t = DateTime->new( year => 1986, month => 1, day => 28,
                    hour => 16, minute => 38,
                    time_zone => 'UTC' );

$t->add( minutes => 1, seconds => 12 );
is( $t->datetime, '1986-01-28T16:39:12', "Adding durations with minutes and seconds works");

$t = DateTime->new( year => 1986, month => 1, day => 28,
                    hour => 16, minute => 38,
                    time_zone => 'UTC' );

$t->add( seconds => 30 );
is( $t->datetime, '1986-01-28T16:38:30', "Adding durations with seconds only works");

$t = DateTime->new( year => 1986, month => 1, day => 28,
                    hour => 16, minute => 38,
                    time_zone => 'UTC' );

$t->add( hours => 1, minutes => 10 );
is( $t->datetime, '1986-01-28T17:48:00', "Adding durations with hours and minutes works");

$t = DateTime->new( year => 1986, month => 1, day => 28,
                    hour => 16, minute => 38,
                    time_zone => 'UTC' );

$t->add( days => 3 );
is( $t->datetime, '1986-01-31T16:38:00', "Adding durations with days only works");


$t = DateTime->new( year => 1986, month => 1, day => 28,
                    hour => 16, minute => 38,
                    time_zone => 'UTC' );

$t->add( days => 3, hours => 2 );
is( $t->datetime, '1986-01-31T18:38:00', "Adding durations with days and hours works");


$t = DateTime->new( year => 1986, month => 1, day => 28,
                    hour => 16, minute => 38,
                    time_zone => 'UTC' );

$t->add( days => 3, hours => 2, minutes => 20, seconds => 15 );
is( $t->datetime, '1986-01-31T18:58:15', "Adding durations with days, hours, minutes, and seconds works");

# Add 15M - this test failed at one point in N::I::Time
$t = DateTime->new( year => 2001, month => 4, day => 5,
                    hour => 16,
                    time_zone => 'UTC' );

$t->add( minutes => 15 );
is( $t->datetime, '2001-04-05T16:15:00', "Adding minutes to an ical string");

# Subtract a duration
$t->add( minutes => -15 );
is( $t->datetime, '2001-04-05T16:00:00', "Back where we started");

undef $t;

$t = DateTime->new( year => 1986, month => 1, day => 28,
                    hour => 16, minute => 38,
                    time_zone => 'UTC' );

$t->add( seconds => 60 );
is( $t->datetime, "1986-01-28T16:39:00", "adding positive seconds with seconds works" );
$t->add( seconds => -120 );
is( $t->datetime, "1986-01-28T16:37:00", "adding negative seconds with seconds works" );

# test sub months
$t = DateTime->new( year => 2001, month => 1, day => 31,
                    time_zone => 'UTC',
                  );
$t->add(days => 1);
is( $t->date, '2001-02-01', 'february 1st' );

$t = DateTime->new( year => 2001, month => 2, day => 28,
                    time_zone => 'UTC',
                  );
$t->add(days => 1);
is( $t->date, '2001-03-01', 'march 1st' );

$t = DateTime->new( year => 2001, month => 3, day => 31,
                    time_zone => 'UTC',
                  );
$t->add(days => 1);
is( $t->date, '2001-04-01', 'april 1st' );

$t = DateTime->new( year => 2001, month => 4, day => 30,
                    time_zone => 'UTC',
                  );
$t->add(days => 1);
is( $t->date, '2001-05-01', 'may 1st' );

$t = DateTime->new( year => 2001, month => 5, day => 31,
                    time_zone => 'UTC',
                  );
$t->add(days => 1);
is( $t->date, '2001-06-01', 'june 1st' );

$t = DateTime->new( year => 2001, month => 6, day => 30,
                    time_zone => 'UTC',
                  );
$t->add(days => 1);
is( $t->date, '2001-07-01', 'july 1st' );

$t = DateTime->new( year => 2001, month => 7, day => 31,
                    time_zone => 'UTC',
                  );
$t->add(days => 1);
is( $t->date, '2001-08-01', 'august 1st' );

$t = DateTime->new( year => 2001, month => 8, day => 31,
                    time_zone => 'UTC',
                  );
$t->add(days => 1);
is( $t->date, '2001-09-01', 'september 1st' );

$t = DateTime->new( year => 2001, month => 9, day => 30,
                    time_zone => 'UTC',
                  );
$t->add(days => 1);
is( $t->date, '2001-10-01', 'october 1st' );

$t = DateTime->new( year => 2001, month => 10, day => 31,
                    time_zone => 'UTC',
                  );
$t->add(days => 1);
is( $t->date, '2001-11-01', 'november 1st' );

$t = DateTime->new( year => 2001, month => 11, day => 30,
                    time_zone => 'UTC',
                  );
$t->add(days => 1);
is( $t->date, '2001-12-01', 'december 1st' );

$t = DateTime->new( year => 2001, month => 12, day => 31,
                    time_zone => 'UTC',
                  );
$t->add(days => 1);
is( $t->date, '2002-01-01', 'january 1st' );

# Adding years

# Before leap day, not a leap year ...
$t = DateTime->new( year => 2001, month => 2, day => 28,
                    time_zone => 'UTC',
                  );
$t->add( years => 1 );
is( $t->date, '2002-02-28', 'Adding a year' );
$t->add( years => 17 );
is( $t->date, '2019-02-28', 'Adding 17 years' );

# After leap day, not a leap year ...
$t = DateTime->new( year => 2001, month => 3, day => 28,
                    time_zone => 'UTC',
                  );
$t->add( years => 1 );
is( $t->date, '2002-03-28', 'Adding a year' );
$t->add( years => 17 );
is( $t->date, '2019-03-28', 'Adding 17 years' );

# On leap day, in a leap year ...
$t = DateTime->new( year => 2000, month => 2, day => 29,
                    time_zone => 'UTC',
                  );
$t->add( years => 1 );
is( $t->date, '2001-03-01', 'Adding a year' );
$t->add( years => 17 );
is( $t->date, '2018-03-01', 'Adding 17 years' );

# Before leap day, in a leap year ...
$t = DateTime->new( year => 2000, month => 2, day => 28,
                    time_zone => 'UTC',
                  );
$t->add( years => 1 );
is( $t->date, '2001-02-28', 'Adding a year' );
$t->add( years => 17 );
is( $t->date, '2018-02-28', 'Adding 17 years' );

# After leap day, in a leap year ...
$t = DateTime->new( year => 2000, month => 3, day => 28,
                    time_zone => 'UTC',
                  );
$t->add( years => 1 );
is( $t->date, '2001-03-28', 'Adding a year' );
$t->add( years => 17 );
is( $t->date, '2018-03-28', 'Adding 17 years' );

# Test a bunch of years, before leap day
for (1..99) {
    $t = DateTime->new( year => 2000, month => 2, day => 28,
                        time_zone => 'UTC',
                      );
    $t->add( years => $_ );
    my $x = sprintf '%02d', $_;
    is( $t->date, "20${x}-02-28", "Adding $_ years");
}

# Test a bunch of years, after leap day
for (1..99) {
    $t = DateTime->new( year => 2000, month => 3, day => 28,
                        time_zone => 'UTC',
                      );
    $t->add( years => $_ );
    my $x = sprintf '%02d', $_;
    is( $t->date, "20${x}-03-28", "Adding $_ years");
}

# And more of the same, starting on a non-leap year

# Test a bunch of years, before leap day
for (1..97) {
    $t = DateTime->new( year => 2002, month => 2, day => 28,
                        time_zone => 'UTC',
                      );
    $t->add( years => $_ );
    my $x = sprintf '%02d', $_ + 2;
    is( $t->date, "20${x}-02-28", "Adding $_ years");
}

# Test a bunch of years, after leap day
for (1..97) {
    $t = DateTime->new( year => 2002, month => 3, day => 28,
                        time_zone => 'UTC',
                      );
    $t->add( years => $_ );
    my $x = sprintf '%02d', $_ + 2;
    is( $t->date, "20${x}-03-28", "Adding $_ years");
}

# subtract years
for (1..97) {
    $t = DateTime->new( year => 1999, month => 3, day => 1,
                        time_zone => 'UTC',
                      );
    $t->add( years => -$_ );
    my $x = sprintf '%02d', 99 - $_;
    is( $t->date, "19${x}-03-01", "Subtracting $_ years");
}

# test some old bugs

# bug adding months where current month + months added were > 25
$t = DateTime->new( year => 1997, month => 12, day => 1,
                    time_zone => 'UTC',
                  );
$t->add( months => 14 );
is( $t->date, '1999-02-01', 'Adding months--rollover year' );

# bug subtracting months with year rollover
$t = DateTime->new( year => 1997, month => 1, day => 1,
                    time_zone => 'UTC',
                  );
$t->add( months => -1 );
is( $t->date, '1996-12-01', 'Subtracting months--rollover year' );

my $new = $t + DateTime::Duration->new( years => 2 );
is( $new->date, '1998-12-01', 'test + overloading' );

# test nanoseconds

{
    my $t = DateTime->new( year => 1997, month => 1, day => 1,
                           hour => 1, minute => 1, second => 59,
                           nanosecond => 500000000,
                           time_zone => 'UTC',
                         );

    $t->add( nanoseconds => 500000000 );
    is( $t->second, 0, 'fractional second rollover' );
    $t->add( nanoseconds => 123000000 );
    is( $t->fractional_second, 0.123, 'as fractional_second' );
}

{
    my $dt = DateTime->new( year => 2003, month => 2, day => 28 );
    $dt->add( months => 1, days => 1 );

    is( $dt->ymd, '2003-04-01', 'order of units in date math' );
}

{
    my $dt = DateTime->new( year => 2003, hour => 12, minute => 1);
    $dt->add( minutes => 30, seconds => -1 );

    is( $dt->hour,   12, 'hour is 12' );
    is( $dt->minute, 30, 'minute is 30' );
    is( $dt->second, 59, 'second is 59' );
}
