use strict;
use warnings;

use File::Spec;
use Test::More;

use lib File::Spec->catdir( File::Spec->curdir, 't' );

BEGIN { require 'check_datetime_version.pl' }

use DateTime::TimeZone;

my @names = DateTime::TimeZone::all_names;

my $is_maintainer = -d '.svn' ? 1 : 0;

my $tests_per_zone = $is_maintainer ? 9 : 4;
plan tests => 30 + ( $tests_per_zone * scalar @names );

foreach my $name (@names)
{
    my $tz = DateTime::TimeZone->new( name => $name );
    isa_ok( $tz, 'DateTime::TimeZone' );

    is( $tz->name, $name, 'check ->name' );

    is( $tz->is_floating, 0, 'should not be floating' );
    is( $tz->is_utc, 0, 'should not be UTC' );

    # adding these tests makes the test suite take a _long_ time to
    # finish, and it uses up lots of memory too.
    if ( $is_maintainer )
    {
        my $dt;
        eval { $dt = DateTime->now( time_zone => $name ) };
        is( $@, '', "Can call DateTime->now with $name" );
        eval { $dt->add( years => 50 ) };
        is( $@, '', "Can add 50 years with $name" );
        eval { $dt->subtract( years => 400 ) };
        is( $@, '', "Can subtract 400 years with $name" );
        eval { $dt = DateTime->new( year => 2000, month => 6, hour => 1, time_zone => $name ) };
        is( $dt->hour, 1, 'make sure that local time is always respected' );
        eval { $dt = DateTime->new( year => 2000, month => 12, hour => 1, time_zone => $name ) };
        is( $dt->hour, 1, 'make sure that local time is always respected' );
    }
}

foreach my $name ( '0', 'Z', 'UTC' )
{
    my $tz = DateTime::TimeZone->new( name => $name );
    isa_ok( $tz, 'DateTime::TimeZone' );

    is( $tz->name, 'UTC', 'name should be UTC' );

    is( $tz->is_floating, 0, 'should not be floating' );
    is( $tz->is_utc, 1, 'should be UTC' );
}


my $tz = DateTime::TimeZone->new( name => 'America/Chicago' );

# These tests are odd since we're feeding UTC times into the time zone
# object, which isn't what happens in real usage.  But doing this
# minimizes how much of DateTime.pm needs to work for these tests.
{
    my $dt = DateTime->new( year => 2001,
                            month => 9,
                            day => 10,
                            time_zone => 'UTC',
                          );
    is( $tz->offset_for_datetime($dt), -18000, 'offset should be -18000' );
    is( $tz->short_name_for_datetime($dt), 'CDT', 'name should be CDT' );
}

{
    my $dt = DateTime->new( year => 2001,
                            month => 10,
                            day => 29,
                            time_zone => 'UTC',
                          );
    is( $tz->offset_for_datetime($dt), -21600, 'offset should be -21600' );
    is( $tz->short_name_for_datetime($dt), 'CST', 'name should be CST' );
}

{
    # check that generation works properly
    my $dt = DateTime->new( year => 2200,
                            month => 11,
                            day => 2,
                            time_zone => 'UTC',
                          );
    is( $tz->offset_for_datetime($dt), -18000, 'generated offset should be -1800' );
    is( $tz->short_name_for_datetime($dt), 'CDT', 'generated name should be CDT' );
}

{
    # check that generation works properly
    my $dt = DateTime->new( year => 2200,
                            month => 11,
                            day => 3,
                            time_zone => 'UTC',
                          );
    is( $tz->offset_for_datetime($dt), -21600, 'generated offset should be -21600' );
    is( $tz->short_name_for_datetime($dt), 'CST', 'generated name should be CST' );
}

{
    # bug when creating new datetime for year just after time zone's
    # max year
    my $tz = DateTime::TimeZone->new( name => 'America/Los_Angeles' );

    my $dt = eval { DateTime->new( year => $tz->{max_year} + 1,
                                   month => 5,
                                   day => 20,
                                   time_zone => $tz
                                 ) };
    ok( $dt, 'was able to create datetime object' );
}

{
    my $dt = DateTime->new( year => 1944,
                            month => 10,
                            day => 29,
                            time_zone => 'UTC',
                          );
    is( $tz->offset_for_datetime($dt), -18000, 'offset should be -18000' );
    is( $tz->short_name_for_datetime($dt), 'CWT', 'name should be CWT' );
}


{
    my $dt = DateTime->new( year => 1936,
                            month => 3,
                            day => 2,
                            time_zone => 'UTC',
                          );

    is( $tz->offset_for_datetime($dt), -18000, 'offset should be -18000' );
    is( $tz->short_name_for_datetime($dt), 'EST', 'name should be EST' );
}

{
    my $dt = DateTime->new( year => 1883,
                            month => 1,
                            day => 29,
                            time_zone => 'UTC',
                          );

    is( $tz->offset_for_datetime($dt), -21036, 'offset should be -21036' );
    is( $tz->short_name_for_datetime($dt), 'LMT', 'name should be LMT' );
}

{
    {
        package TestHack;

        sub new { bless {} }
        # UTC RD secs == 63518486401
        sub utc_rd_values { ( 735167, 57601 ) }
    }

    # This is to check a bug in DT::TZ::_span_for_datetime, where it
    # was always looking at the LOCAL_END of the current max_span.
    #
    # Australia/Sydney's max_span (before generation) has a LOCAL_END
    # of 63518522400 and UTC_END of 63518486400.  The values above
    # create a utc_rd_seconds value that is after the UTC_END but
    # before the LOCAL_END.
    my $dt = DateTime->from_object( object => TestHack->new );

    eval { $dt->set_time_zone('UTC')->set_time_zone( 'Australia/Sydney' ) };
    ok( ! $@, 'should be able to set time zone' );
    ok( $dt->is_dst, 'is_dst should be true' );
}

{
    my $tz = DateTime::TimeZone->new( name => '-0100' );
    ok( ! $tz->is_olson, 'is_olson is false for offset only time zone' );
}
