#!/usr/bin/perl -w

use strict;

use File::Spec;
use Sys::Hostname;
use Test::More;

use lib File::Spec->catdir( File::Spec->curdir, 't' );

BEGIN { require 'check_datetime_version.pl' }

plan tests => 19;

use DateTime::TimeZone;

{
    # make sure it doesn't find an /etc/localtime file
    $^W = 0;
    local *DateTime::TimeZone::Local::_from_etc_timezone = sub { undef };
    local *DateTime::TimeZone::Local::_from_etc_TIMEZONE = sub { undef };
    local *DateTime::TimeZone::Local::_from_etc_localtime = sub { undef };
    local *DateTime::TimeZone::Local::_read_etc_sysconfig_clock = sub { undef };
    local *DateTime::TimeZone::Local::_read_etc_default_init = sub { undef };
    $^W = 1;

    local $ENV{TZ} = 'this will not work';

    my $tz;
    eval { $tz = DateTime::TimeZone->new( name => 'local' ) };
    like( $@, qr/cannot determine local time zone/i,
          'invalid time zone name in $ENV{TZ} and no other info available should die' );

    local $ENV{TZ} = '123/456';

    eval { $tz = DateTime::TimeZone->new( name => 'local' ) };
    like( $@, qr/cannot determine local time zone/i,
          'invalid time zone name in $ENV{TZ} and no other info available should die' );
}

{
    $^W = 0;
    local *DateTime::TimeZone::Local::_from_etc_localtime = sub { undef };
    local *DateTime::TimeZone::Local::_read_etc_sysconfig_clock = sub { undef };
    local *DateTime::TimeZone::Local::_read_etc_default_init = sub { undef };
    local *DateTime::TimeZone::Local::_local_from_etc_timezone = sub { undef };
    $^W = 1;

    local $ENV{TZ} = 'Africa/Kinshasa';

    my $tz;
    eval { $tz = DateTime::TimeZone->new( name => 'local' ) };
    is( $@, '', 'valid time zone name in $ENV{TZ} should not die' );
    isa_ok( $tz, 'DateTime::TimeZone::Africa::Kinshasa' );
}

SKIP:
{
    skip "/etc/localtime is not a symlink", 2
        unless -l '/etc/localtime';

    $^W = 0;
    local *DateTime::TimeZone::Local::_readlink = sub { '/usr/share/zoneinfo/US/Eastern' };
    local *DateTime::TimeZone::Local::_from_etc_timezone = sub { undef };
    local *DateTime::TimeZone::Local::_from_etc_TIMEZONE = sub { undef };
    local *DateTime::TimeZone::Local::_read_etc_sysconfig_clock = sub { undef };
    local *DateTime::TimeZone::Local::_read_etc_default_init = sub { undef };
    $^W = 1;

    local $ENV{TZ} = '';

    my $tz;
    eval { $tz = DateTime::TimeZone->new( name => 'local' ) };
    is( $@, '', 'valid time zone name in /etc/localtime should not die' );
    isa_ok( $tz, 'DateTime::TimeZone::America::New_York' );
}

SKIP:
{
    skip "cannot read /etc/sysconfig/clock", 2
        unless -r '/etc/sysconfig/clock' && -f _;

    $^W = 0;
    local *DateTime::TimeZone::Local::_from_etc_localtime = sub { undef };
    local *DateTime::TimeZone::Local::_from_etc_timezone = sub { undef };
    local *DateTime::TimeZone::Local::_from_etc_TIMEZONE = sub { undef };
    local *DateTime::TimeZone::Local::_read_etc_sysconfig_clock = sub { 'US/Eastern' };
    local *DateTime::TimeZone::Local::_read_etc_default_init = sub { undef };
    $^W = 1;

    local $ENV{TZ} = '';

    my $tz;
    eval { $tz = DateTime::TimeZone->new( name => 'local' ) };
    is( $@, '', 'valid time zone name in /etc/sysconfig/clock should not die' );
    isa_ok( $tz, 'DateTime::TimeZone::America::New_York' );
}

SKIP:
{
    skip "cannot read /etc/default/init", 2
        unless -r '/etc/default/init' && -f _;

    $^W = 0;
    local *DateTime::TimeZone::Local::_from_etc_localtime = sub { undef };
    local *DateTime::TimeZone::Local::_from_etc_timezone = sub { undef };
    local *DateTime::TimeZone::Local::_from_etc_TIMEZONE = sub { undef };
    local *DateTime::TimeZone::Local::_read_etc_sysconfig_clock = sub { undef };
    local *DateTime::TimeZone::Local::_read_etc_default_init = sub { 'US/Eastern' };
    $^W = 1;

    local $ENV{TZ} = '';

    my $tz;
    eval { $tz = DateTime::TimeZone->new( name => 'local' ) };
    is( $@, '', 'valid time zone name in /etc/default/init should not die' );
    isa_ok( $tz, 'DateTime::TimeZone::America::New_York' );
}

SKIP:
{
    skip "Cannot run these tests without explicitly knowing local time zone first (only runs on developers' machine)", 6
        unless hostname =~ /houseabsolute|quasar/ && -d 'CVS';

    {
        local $ENV{TZ} = '';

        my $tz;
        eval { $tz = DateTime::TimeZone->new( name => 'local' ) };
        is( $@, '', 'valid time zone name in /etc/localtime should not die' );
        isa_ok( $tz, 'DateTime::TimeZone::America::Chicago' );
    }

    {
        $^W = 0;
        local *DateTime::TimeZone::Local::_from_etc_localtime = sub { undef };
        $^W = 1;

        my $tz;
        eval { $tz = DateTime::TimeZone->new( name => 'local' ) };
        is( $@, '', 'valid time zone name in /etc/timezone should not die' );
        isa_ok( $tz, 'DateTime::TimeZone::America::Chicago' );
    }

    {
        # requires that /etc/default/init contain
        # TZ=Australia/Melbourne to work.
        $^W = 0;
        local *DateTime::TimeZone::Local::_from_etc_localtime = sub { undef };
        local *DateTime::TimeZone::Local::_from_etc_timezone = sub { undef };
        local *DateTime::TimeZone::Local::_from_etc_TIMEZONE = sub { undef };
        $^W = 1;

        my $tz;
        eval { $tz = DateTime::TimeZone->new( name => 'local' ) };
        is( $@, '', '/etc/default/init contains TZ=Australia/Melbourne' );
        isa_ok( $tz, 'DateTime::TimeZone::Australia::Melbourne' );
    }
}

SKIP:
{
    skip "These tests are too dangerous to run on someone else's machine ;)", 3
        unless hostname =~ /houseabsolute|quasar/ && -d 'CVS';

    skip "These tests can only be run if we can overwrite /etc/localtime", 3
        unless -w '/etc/localtime' && -l '/etc/localtime';

    my $tz_file = readlink '/etc/localtime';

    unlink '/etc/localtime' or die "Cannot unlink /etc/localtime: $!";

    require File::Copy;
    File::Copy::copy( '/usr/share/zoneinfo/Asia/Calcutta', '/etc/localtime' )
        or die "Cannot copy /usr/share/zoneinfo/Asia/Calcutta to '/etc/localtime': $!";

    {
        local $ENV{TZ} = '';

        require Cwd;
        my $cwd = Cwd::cwd();

        my $tz;
        eval { $tz = DateTime::TimeZone->new( name => 'local' ) };
        is( $@, '', 'copy of zoneinfo file at /etc/localtime' );
        isa_ok( $tz, 'DateTime::TimeZone::Asia::Calcutta' );

        is( Cwd::cwd(), $cwd, 'cwd should not change after finding local time zone' );
    }

    unlink '/etc/localtime' or die "Cannot unlink /etc/localtime: $!";
    symlink $tz_file, '/etc/localtime'
        or die "Cannot symlink $tz_file to '/etc/localtime': $!";
}
