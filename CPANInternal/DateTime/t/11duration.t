#!/usr/bin/perl -w

use strict;

use Test::More tests => 128;

use DateTime;
use DateTime::Duration;

{
    my %pairs = ( years   => 1,
                  months  => 2,
                  weeks   => 3,
                  days    => 4,
                  hours   => 6,
                  minutes => 7,
                  seconds => 8,
                  nanoseconds => 9,
                );

    my $dur = DateTime::Duration->new(%pairs);

    while ( my ($unit, $val) = each %pairs )
    {
        is( $dur->$unit(), $val, "$unit should be $val" );
    }

    is( $dur->delta_months, 14, "delta_months" );
    is( $dur->delta_days, 25, "delta_days" );
    is( $dur->delta_minutes, 367, "delta_minutes" );
    is( $dur->delta_seconds, 8, "delta_seconds" );
    is( $dur->delta_nanoseconds, 9, "delta_nanoseconds" );

    is( $dur->in_units( 'months' ), 14, "in_units months" );
    is( $dur->in_units( 'days' ), 25, "in_units days" );
    is( $dur->in_units( 'minutes' ), 367, "in_units minutes" );
    is( $dur->in_units( 'seconds' ), 8, "in_units seconds" );
    is( $dur->in_units( 'nanoseconds', 'seconds' ), 9,
        "in_units nanoseconds, seconds" );

    is( $dur->in_units( 'years' ), 1, "in_units years" );
    is( $dur->in_units( 'months', 'years' ), 2, "in_units months, years" );
    is( $dur->in_units( 'weeks' ), 3, "in_units weeks" );
    is( $dur->in_units( 'days', 'weeks' ), 4, "in_units days, weeks" );
    is( $dur->in_units( 'hours' ), 6, "in_units hours" );
    is( $dur->in_units( 'minutes', 'hours' ), 7, "in_units minutes, hours" );
    is( $dur->in_units( 'nanoseconds' ), 8_000_000_009, "in_units nanoseconds" );

    my ( $years, $months, $weeks, $days, $hours,
         $minutes, $seconds, $nanoseconds) =
             $dur->in_units( qw( years months weeks days hours
                                 minutes seconds nanoseconds ) );

    is( $years,       1, "in_units years, list context" );
    is( $months,      2, "in_units months, list context" );
    is( $weeks,       3, "in_units weeks, list context" );
    is( $days,        4, "in_units days, list context" );
    is( $hours,       6, "in_units hours, list context" );
    is( $minutes,     7, "in_units minutes, list context" );
    is( $seconds,     8, "in_units seconds, list context" );
    is( $nanoseconds, 9, "in_units nanoseconds, list context" );

    ok( $dur->is_positive, "should be positive" );
    ok( ! $dur->is_zero, "should not be zero" );
    ok( ! $dur->is_negative, "should not be negative" );

    ok( $dur->is_wrap_mode, "wrap mode" );
}
{
    my %pairs = ( years   => 1,
                  months  => 2,
                  weeks   => 3,
                  days    => 4,
                  hours   => 6,
                  minutes => 7,
                  seconds => 8,
                  nanoseconds => 9,
                );

    my $dur = DateTime::Duration->new( %pairs, end_of_month => 'limit' );

    my $calendar_dur = $dur->calendar_duration;
    is( $calendar_dur->delta_months, 14, "date - delta_months is 14" );
    is( $calendar_dur->delta_minutes, 0, "date - delta_minutes is 0" );
    is( $calendar_dur->delta_seconds, 0, "date - delta_seconds is 0" );
    is( $calendar_dur->delta_nanoseconds, 0, "date - delta_nanoseconds is 0" );
    ok( $calendar_dur->is_limit_mode, "limit mode" );

    my $clock_dur = $dur->clock_duration;
    is( $clock_dur->delta_months, 0, "time  - delta_months is 0" );
    is( $clock_dur->delta_minutes, 367, "time  - delta_minutes is 367" );
    is( $clock_dur->delta_seconds, 8, "time  - delta_seconds is 8" );
    is( $clock_dur->delta_nanoseconds, 9, "time  - delta_nanoseconds is 9" );
    ok( $clock_dur->is_limit_mode, "limit mode" );
}

{
    my $dur = DateTime::Duration->new( days => 1, end_of_month => 'limit' );
    ok( $dur->is_limit_mode, "limit mode" );
}

{
    my $dur = DateTime::Duration->new( days => 1, end_of_month => 'preserve' );
    ok( $dur->is_preserve_mode, "preserve mode" );
}

my $leap_day = DateTime->new( year => 2004, month => 2, day => 29,
                              time_zone => 'UTC',
                            );

{
    my $new =
        $leap_day + DateTime::Duration->new( years => 1,
                                             end_of_month => 'wrap' );

    is( $new->date, '2005-03-01', "new date should be 2005-03-01" );
}

{
    my $new =
        $leap_day + DateTime::Duration->new( years => 1,
                                             end_of_month => 'limit' );

    is( $new->date, '2005-02-28', "new date should be 2005-02-28" );
}


{
    my $new =
        $leap_day + DateTime::Duration->new( years => 1,
                                             end_of_month => 'preserve' );

    is( $new->date, '2005-02-28', "new date should be 2005-02-28" );

    my $new2 =
        $leap_day + DateTime::Duration->new( months => 1,
                                             end_of_month => 'preserve' );
    is( $new2->date, '2004-03-31', "new date should be 2004-03-31" );
}

{
    my $inverse =
        DateTime::Duration->new( years => 1, months => 1,
                                 weeks => 1, days => 1,
                                 hours => 1, minutes => 2, seconds => 3, )->inverse;

    is( $inverse->years, 1, 'inverse years should be positive' );
    is( $inverse->months, 1, 'inverse months should be positive' );
    is( $inverse->weeks, 1, 'inverse weeks should be positive' );
    is( $inverse->days, 1, 'inverse days should be positive' );
    is( $inverse->hours, 1, 'inverse hours should be positive' );
    is( $inverse->minutes, 2, 'inverse minutes should be positive' );
    is( $inverse->seconds, 3, 'inverse minutes should be positive' );

    is( $inverse->delta_months, -13, 'inverse delta months should be negative' );
    is( $inverse->delta_days, -8, 'inverse delta months should be negative' );
    is( $inverse->delta_minutes, -62, 'inverse delta minutes should be negative' );
    is( $inverse->delta_seconds, -3, 'inverse delta seconds should be negative' );

    ok( $inverse->is_negative, "should be negative" );
    ok( ! $inverse->is_zero, "should not be zero" );
    ok( ! $inverse->is_positive, "should not be positivea" );
}

{
    my $dur1 = DateTime::Duration->new( months => 6, days => 10 );

    my $dur2 = DateTime::Duration->new( months => 3, days => 7 );

    my $new1 = $dur1 + $dur2;
    is( $new1->delta_months, 9, 'test + overloading' );
    is( $new1->delta_days, 17, 'test + overloading' );

    my $new2 = $dur1 - $dur2;
    is( $new2->delta_months, 3, 'test - overloading' );
    is( $new2->delta_days, 3, 'test - overloading' );

    my $new3 = $dur2 - $dur1;
    is( $new3->delta_months, -3, 'test - overloading' );
    is( $new3->delta_days, -3, 'test - overloading' );
}

{
    my $dur1 = DateTime::Duration->new( months => 6, days => 10 );

    my $new1 = $dur1 * 4;
    is( $new1->delta_months, 24, 'test * overloading' );
    is( $new1->delta_days, 40, 'test * overloading' );

    $dur1->multiply(4);
    is( $dur1->delta_months, 24, 'test multiply' );
    is( $dur1->delta_days, 40, 'test multiply' );
}

{
    my $dur1 =
        DateTime::Duration->new
            ( months => 6, days => 10, seconds => 3, nanoseconds => 1_200_300_400 );

    my $dur2 = DateTime::Duration->new( seconds => 1, nanoseconds => 500_000_000 );

    is( $dur1->delta_seconds, 4, 'test nanoseconds overflow' );
    is( $dur1->delta_nanoseconds, 200_300_400, 'test nanoseconds remainder' );

    my $new1 = $dur1 - $dur2;

    is( $new1->delta_seconds, 2, 'seconds is positive' );
    is( $new1->delta_nanoseconds, 700_300_400, 'nanoseconds remainder is negative' );

    $new1->add( nanoseconds => 500_000_000 );
    is( $new1->delta_seconds, 3, 'seconds are unaffected' );
    is( $new1->delta_nanoseconds, 200_300_400, 'nanoseconds are back' );

    my $new2 = $dur1 - $dur2;
    $new2->add( nanoseconds => 1_500_000_000 );
    is( $new2->delta_seconds, 4, 'seconds go up' );
    is( $new2->delta_nanoseconds, 200_300_400, 'nanoseconds are normalized' );

    $new2->subtract( nanoseconds => 100_000_000 );
    is( $new2->delta_nanoseconds, 100_300_400, 'sub nanoseconds works' );

    my $new3 = $dur2 * 3;

    is( $new3->delta_seconds, 4, 'seconds normalized after multiplication');
    is( $new3->delta_nanoseconds, 500_000_000,
        'nanoseconds normalized after multiplication' );
}

{
    my $dur = DateTime::Duration->new( nanoseconds => -10 );
    is( $dur->nanoseconds, 10, 'nanoseconds is 10' );
    is( $dur->delta_nanoseconds, -10, 'delta_nanoseconds is -10' );
    ok( $dur->is_negative, 'duration is negative' );
}

{
    my $dur = DateTime::Duration->new( days => 0 );
    is( $dur->delta_days, 0, 'delta_days is 0' );
    ok( ! $dur->is_positive, 'not positive' );
    ok( $dur->is_zero, 'is zero' );
    ok( ! $dur->is_negative, 'not negative' );
}

{
    eval { DateTime::Duration->new( months => 3 )->add( hours => -3 )->add( minutes => 1 ) };
    ok( ! $@, 'method chaining should work' );
}

{
    my $min_1  = DateTime::Duration->new( minutes => 1 );
    my $hour_1 = DateTime::Duration->new( hours => 1 );

    my $min_59 = $hour_1 - $min_1;

    is( $min_59->delta_months,   0, 'delta_months is 0' );
    is( $min_59->delta_days,     0, 'delta_days is 0' );
    is( $min_59->delta_minutes, 59, 'delta_minutes is 59' );
    is( $min_59->delta_seconds,  0, 'delta_seconds is 0' );
    is( $min_59->delta_nanoseconds, 0, 'delta_nanoseconds is 0' );

    my $min_neg_59 = $min_1 - $hour_1;

    is( $min_neg_59->delta_months,    0, 'delta_months is 0' );
    is( $min_neg_59->delta_days,      0, 'delta_days is 0' );
    is( $min_neg_59->delta_minutes, -59, 'delta_minutes is -59' );
    is( $min_neg_59->delta_seconds,   0, 'delta_seconds is 0' );
    is( $min_neg_59->delta_nanoseconds, 0, 'delta_nanoseconds is 0' );
}

{
    my $dur1 = DateTime::Duration->new( minutes => 10 );
    my $dur2 = DateTime::Duration->new( minutes => 20 );

    eval { my $x = 1 if $dur1 <=> $dur2 };
    like( $@, qr/does not overload comparison/,
          'check error for duration comparison overload' );

    is( DateTime::Duration->compare( $dur1, $dur2 ), -1,
        '20 minutes is greater than 10 minutes' );

    is( DateTime::Duration->compare( $dur1, $dur2, DateTime->new( year => 1 ) ), -1,
        '20 minutes is greater than 10 minutes' );
}


{
    my $dur1 = DateTime::Duration->new( days   => 29 );
    my $dur2 = DateTime::Duration->new( months => 1 );

    my $base = DateTime->new( year => 2004 );
    is( DateTime::Duration->compare( $dur1, $dur2, $base ), -1,
        '29 days is less than 1 month with base of 2004-01-01' );

    $base = DateTime->new( year => 2004, month => 2 );
    is( DateTime::Duration->compare( $dur1, $dur2, $base ), 0,
        '29 days is equal to 1 month with base of 2004-02-01' );

    $base = DateTime->new( year => 2005, month => 2 );
    is( DateTime::Duration->compare( $dur1, $dur2, $base ), 1,
        '29 days is greater than 1 month with base of 2005-02-01' );
}

{
    my $dur1 = DateTime::Duration->new( nanoseconds => 1_000,
                                        seconds     => 1,
                                      );

    my $dur2 = $dur1->clone->subtract( nanoseconds => 5_000 );

    is( $dur2->delta_seconds, 0, 'normalize nanoseconds to positive' );
    is( $dur2->delta_nanoseconds, 999_996_000, 'normalize nanoseconds to positive' );

    my $dur3 =
        $dur1->clone->subtract( nanoseconds => 6_000 )->subtract( nanoseconds => 999_999_000 );

    is( $dur3->delta_seconds, 0, 'normalize nanoseconds to negative' );
    is( $dur3->delta_nanoseconds, -4_000, 'normalize nanoseconds to negative' );

    my $dur4 = DateTime::Duration->new( seconds => -1,
                                        nanoseconds => -2_500_000_000
                                      );

    is( $dur4->delta_seconds, -3, 'normalize many negative nanoseconds' );
    is( $dur4->delta_nanoseconds, -500_000_000, 'normalize many negative nanoseconds' );
}

{
    my $dur = DateTime::Duration->new( minutes => 30,
                                       seconds => -1,
                                     );

    ok( ! $dur->is_positive, 'is not positive' );
    ok( ! $dur->is_zero,     'is not zero' );
    ok( ! $dur->is_negative, 'is not negative' );
}

{
    my $dur = DateTime::Duration->new( minutes => 50 );

    is( $dur->in_units('years'), 0, 'in_units returns 0 for years' );
    is( $dur->in_units('months'), 0, 'in_units returns 0 for months' );
    is( $dur->in_units('days'), 0, 'in_units returns 0 for days' );
    is( $dur->in_units('hours'), 0, 'in_units returns 0 for hours' );
    is( $dur->in_units('seconds'), 0, 'in_units returns 0 for seconds' );
    is( $dur->in_units('nanoseconds'), 0, 'in_units returns 0 for nanoseconds' );
}

{
    local $TODO = 'reject fractional units in DateTime::Duration->new';

    eval { DateTime::Duration->new( minutes => 50.2 ) };

    like( $@, qr/is an integer/, 'cannot create a duration with fractional units' );
}
