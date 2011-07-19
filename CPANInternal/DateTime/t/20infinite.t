#!/usr/bin/perl -w

use strict;

use Test::More tests => 42;

use DateTime;

my $pos = DateTime::Infinite::Future->new;
my $neg = DateTime::Infinite::Past->new;
my $posinf = DateTime::INFINITY;
my $neginf = DateTime::NEG_INFINITY;
my $nan_string = DateTime::NAN;

# infinite date math
{
    ok( $pos->is_infinite, 'positive infinity should be infinite' );
    ok( $neg->is_infinite, 'negative infinity should be infinite' );
    ok( !$pos->is_finite, 'positive infinity should not be finite' );
    ok( !$neg->is_finite, 'negative infinity should not be finite' );

    # that's a long time ago!
    my $long_ago = DateTime->new( year => -100_000 );

    ok( $neg < $long_ago,
        'negative infinity is really negative' );

    my $far_future = DateTime->new( year => 100_000 );
    ok( $pos > $far_future,
        'positive infinity is really positive' );

    ok( $pos > $neg,
        'positive infinity is bigger than negative infinity' );

    my $pos_dur = $pos - $far_future;
    ok( $pos_dur->is_positive,
        'infinity - normal = infinity' );

    my $pos2 = $long_ago + $pos_dur;
    ok( $pos2 == $pos,
        'normal + infinite duration = infinity' );

    my $neg_dur = $far_future - $pos;
    ok( $neg_dur->is_negative,
        'normal - infinity = neg infinity' );

    my $neg2 = $long_ago + $neg_dur;
    ok( $neg2 == $neg,
            'normal + neg infinite duration = neg infinity' );

    my $dur = $pos - $pos;
    my %deltas = $dur->deltas;
    my @compare = qw( days seconds nanoseconds );
    foreach (@compare)
    {
        # NaN != NaN (but should stringify the same)
        is( $deltas{$_} . '', $nan_string,
            "infinity - infinity = nan ($_)" );
    }

    my $new_pos = $pos->clone->add( days => 10 );
    ok( $new_pos == $pos,
            "infinity + normal duration = infinity" );

    my $new_pos2 = $pos->clone->subtract( days => 10 );
    ok( $new_pos2 == $pos,
            "infinity - normal duration = infinity" );

    ok( $pos == $posinf,
            "infinity (datetime) == infinity (number)" );

    ok( $neg == $neginf,
            "neg infinity (datetime) == neg infinity (number)" );
}

# This could vary across platforms
my $pos_as_string = $posinf . '';
my $neg_as_string = $neginf . '';

# formatting
{
    foreach my $m ( qw( year month day hour minute second
                        microsecond millisecond nanosecond ) )
    {
        is( $pos->$m() . '', $pos_as_string,
            "pos $m is $pos_as_string" );

        is( $neg->$m() . '', $neg_as_string,
            "neg $m is $pos_as_string" );
    }
}

{
    my $now  = DateTime->now;

    is( DateTime->compare($pos, $now),  1, 'positive infinite is greater than now' );
    is( DateTime->compare($neg, $now), -1, 'negative infinite is less than now' );
}

{
    my $now = DateTime->now;
    my $pos2 = $pos + DateTime::Duration->new( months => 1 );

    ok( $pos == $pos2,
            "infinity (datetime) == infinity (datetime)" );
}

{
    my $now = DateTime->now;
    my $neg2 = $neg + DateTime::Duration->new( months => 1 );

    ok( $neg == $neg2,
        "-infinity (datetime) == -infinity (datetime)" );
}

{
    cmp_ok( "$pos", 'eq', $posinf, 'stringified infinity (datetime) eq infinity (number)' );
    cmp_ok( "$neg", 'eq', $neginf, 'stringified neg infinity (datetime) eq neg infinity (number)' );
}
