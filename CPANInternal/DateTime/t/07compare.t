#!/usr/bin/perl -w

use strict;

use Test::More tests => 26;

use DateTime;

my $date1 = DateTime->new( year => 1997, month => 10, day => 24,
                           hour => 12, minute => 0, second => 0,
                           time_zone => 'UTC' );
my $date2 = DateTime->new( year => 1997, month => 10, day => 24,
                           hour => 12, minute => 0, second => 0,
                           time_zone => 'UTC' );


# make sure that comparing to itself eq 0
my $identity = $date1->compare($date2);
ok($identity == 0, "Identity comparison");

$date2 = DateTime->new( year => 1997, month => 10, day => 24,
                        hour => 12, minute => 0, second => 1,
                        time_zone => 'UTC' );
ok($date1->compare($date2) == -1, 'Comparison $a < $b, 1 second diff');

$date2 = DateTime->new( year => 1997, month => 10, day => 24,
                        hour => 12, minute => 1, second => 0,
                        time_zone => 'UTC' );
ok($date1->compare($date2) == -1, 'Comparison $a < $b, 1 minute diff');

$date2 = DateTime->new( year => 1997, month => 10, day => 24,
                        hour => 13, minute => 0, second => 0,
                        time_zone => 'UTC' );
ok($date1->compare($date2) == -1, 'Comparison $a < $b, 1 hour diff');

$date2 = DateTime->new( year => 1997, month => 10, day => 25,
                        hour => 12, minute => 0, second => 0,
                        time_zone => 'UTC' );
ok($date1->compare($date2) == -1, 'Comparison $a < $b, 1 day diff');

$date2 = DateTime->new( year => 1997, month => 11, day => 24,
                        hour => 12, minute => 0, second => 0,
                        time_zone => 'UTC' );
ok($date1->compare($date2) == -1, 'Comparison $a < $b, 1 month diff');

$date2 = DateTime->new( year => 1998, month => 10, day => 24,
                        hour => 12, minute => 0, second => 0,
                        time_zone => 'UTC' );
ok($date1->compare($date2) == -1, 'Comparison $a < $b, 1 year diff');

# $a > $b tests

$date2 = DateTime->new( year => 1997, month => 10, day => 24,
                        hour => 11, minute => 59, second => 59,
                        time_zone => 'UTC' );
ok($date1->compare($date2) == 1, 'Comparison $a > $b, 1 second diff');

$date2 = DateTime->new( year => 1997, month => 10, day => 24,
                        hour => 11, minute => 59, second => 0,
                        time_zone => 'UTC' );
ok($date1->compare($date2) == 1, 'Comparison $a > $b, 1 minute diff');

$date2 = DateTime->new( year => 1997, month => 10, day => 24,
                        hour => 11, minute => 0, second => 0,
                        time_zone => 'UTC' );
ok($date1->compare($date2) == 1, 'Comparison $a > $b, 1 hour diff');

$date2 = DateTime->new( year => 1997, month => 10, day => 23,
                        hour => 12, minute => 0, second => 0,
                        time_zone => 'UTC' );
ok($date1->compare($date2) == 1, 'Comparison $a > $b, 1 day diff');

$date2 = DateTime->new( year => 1997, month => 9, day => 24,
                        hour => 12, minute => 0, second => 0,
                        time_zone => 'UTC' );
ok($date1->compare($date2) == 1, 'Comparison $a > $b, 1 month diff');

$date2 = DateTime->new( year => 1996, month => 10, day => 24,
                        hour => 12, minute => 0, second => 0,
                        time_zone => 'UTC' );
ok($date1->compare($date2) == 1, 'Comparison $a > $b, 1 year diff');


my $infinity = DateTime::INFINITY;

ok($date1->compare($infinity) == -1, 'Comparison $a < inf');

ok($date1->compare(-$infinity) == 1, 'Comparison $a > -inf');

# comparison overloading, and infinity

ok( ($date1 <=> $infinity) == -1, 'Comparison overload $a <=> inf');

ok( ($infinity <=> $date1) == 1, 'Comparison overload $inf <=> $a');

# comparison with floating time
{
    my $date1 = DateTime->new( year => 1997, month => 10, day => 24,
                               hour => 12, minute => 0, second => 0,
                               time_zone => 'America/Chicago' );
    my $date2 = DateTime->new( year => 1997, month => 10, day => 24,
                               hour => 12, minute => 0, second => 0,
                               time_zone => 'floating' );

    is( DateTime->compare( $date1, $date2 ), 0, 'Comparison with floating time (cmp)' );
    is( ($date1 <=> $date2), 0, 'Comparison with floating time (<=>)' );
    is( ($date1 cmp $date2), 0, 'Comparison with floating time (cmp)' );
    is( DateTime->compare_ignore_floating( $date1, $date2 ), 1,
        'Comparison with floating time (cmp)' );
}

# sub-second
{
    my $date1 = DateTime->new( year => 1997, month => 10, day => 24,
                               hour => 12, minute => 0, second => 0,
                               nanosecond => 100,
                             );

    my $date2 = DateTime->new( year => 1997, month => 10, day => 24,
                               hour => 12, minute => 0, second => 0,
                               nanosecond => 200,
                             );

    is( DateTime->compare( $date1, $date2 ), -1, 'Comparison with floating time (cmp)' );
    is( ($date1 <=> $date2), -1, 'Comparison with floating time (<=>)' );
    is( ($date1 cmp $date2), -1, 'Comparison with floating time (cmp)' );
}

{
    package DT::Test;

    sub new { shift; bless [@_] }

    sub utc_rd_values { @{ $_[0] } }
}

{
    my $dt = DateTime->new( year => 1950 );
    my @values = $dt->utc_rd_values;

    $values[2] += 50;

    my $dt_test1 = DT::Test->new( @values );

    ok( $dt < $dt_test1, 'comparison works across different classes' );

    $values[0] -= 1;

    my $dt_test2 = DT::Test->new( @values );

    ok( $dt > $dt_test2, 'comparison works across different classes' );
}
