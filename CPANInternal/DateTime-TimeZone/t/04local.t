use strict;
use warnings;

use DateTime::TimeZone::Local;
use DateTime::TimeZone::Local::Unix;
use File::Basename qw( basename );
use File::Spec;
use Sys::Hostname qw( hostname );
use Test::More;

plan skip_all => 'HPUX is weird'
    if $^O eq 'hpux';

use lib File::Spec->catdir( File::Spec->curdir, 't' );

BEGIN { require 'check_datetime_version.pl' }

my $IsMaintainer = hostname() =~ /houseabsolute|quasar/ && -d '.svn';
my $CanWriteEtcLocaltime = -w '/etc/localtime' && -l '/etc/localtime';

my @aliases = sort keys %{ DateTime::TimeZone::links() };
my @names = DateTime::TimeZone::all_names();

plan tests => @aliases + @names + 31;


# Ensures that we can load our OS-specific subclass. Otherwise this
# might happen later in an eval, and the error will get lost.
DateTime::TimeZone::Local->_load_subclass();

{
    my %links = DateTime::TimeZone->links();

    for my $alias ( sort @aliases )
    {
        local $ENV{TZ} = $alias;
        my $tz = eval { DateTime::TimeZone::Local->TimeZone() };
        is( $tz->name(), $links{$alias},
            "$alias in \$ENV{TZ} for Local->TimeZone()" );
    }
}

{
    for my $name ( sort @names )
    {
        local $ENV{TZ} = $name;
        my $tz = eval { DateTime::TimeZone::Local->TimeZone() };
        is( $tz->name(), $name,
            "$name in \$ENV{TZ} for Local->TimeZone()" );
    }
}

{
    local $ENV{TZ} = 'this will not work';

    my $tz = DateTime::TimeZone::Local::Unix->FromEnv();
    is( $tz, undef,
        'invalid time zone name in $ENV{TZ} fails' );

    local $ENV{TZ} = '123/456';

    $tz = DateTime::TimeZone::Local::Unix->FromEnv();
    is( $tz, undef,
        'invalid time zone name in $ENV{TZ} fails' );
}

{
    local $ENV{TZ} = 'Africa/Kinshasa';

    my $tz = DateTime::TimeZone::Local::Unix->FromEnv();
    is( $tz->name(), 'Africa/Kinshasa', 'tz object name() is Africa::Kinshasa' );

    local $ENV{TZ} = 0;
    $tz = eval { DateTime::TimeZone::Local->TimeZone() };
    is( $tz->name(), 'UTC',
        "\$ENV{TZ} set to 0 returns UTC" );
}


{
    # This passes the _IsValidName() check but when passed to
    # DT::TZ->new() will throw an exception.
    {
        package Foo;
        use overload '""' => sub { "Foo" },
                     'eq' => sub { "$_[0]" eq "$_[1]" };
    }
    local $ENV{TZ} = bless [], 'Foo';

    DateTime::TimeZone::Local::Unix->FromEnv();
    is( $@, '', 'FromEnv does not leave $@ set' );
}

{
    local $^O = 'DoesNotExist';
    my @err;
    local $SIG{__DIE__} = sub { push @err, shift };

    eval { DateTime::TimeZone::Local->_load_subclass() };

    is_deeply( \@err, [],
               'error loading local time zone module is not seen by __DIE__ handler' );
}

no warnings 'redefine';

SKIP:
{
    skip "/etc/localtime is not a symlink", 6
        unless -l '/etc/localtime';

    local *DateTime::TimeZone::Local::Unix::_Readlink = sub { '/usr/share/zoneinfo/US/Eastern' };

    my $tz;
    eval { $tz = DateTime::TimeZone::Local::Unix->FromEtcLocaltime() };
    is( $@, '', 'valid time zone name in /etc/localtime symlink should not die' );
    is( $tz->name(), 'America/New_York',
        'FromEtchLocaltime() with _Readlink returning /usr/share/zoneinfo/US/Eastern' );

    local *DateTime::TimeZone::Local::Unix::_Readlink = sub { '/usr/share/zoneinfo/Foo/Bar' };

    $tz = DateTime::TimeZone::Local::Unix->FromEtcLocaltime() ;
    is( $@, '', 'valid time zone name in /etc/localtime symlink should not leave $@ set' );
    ok( ! $tz, 'no time zone was found' );


    local *DateTime::TimeZone::Local::Unix::_Readlink = sub { undef };
    local *DateTime::TimeZone::Local::Unix::_FindMatchingZoneinfoFile = sub { 'America/Los_Angeles' };

    eval { $tz = DateTime::TimeZone::Local::Unix->FromEtcLocaltime() };
    is( $@, '', 'fall back to _FindMatchZoneinfoFile if _Readlink finds nothing' );
    is( $tz->name(), 'America/Los_Angeles',
        'FromEtchLocaltime() with _FindMatchingZoneinfoFile returning America/Los_Angeles' );
}

SKIP:
{
    skip "cannot read /etc/sysconfig/clock", 2
        unless -r '/etc/sysconfig/clock' && -f _;

    local *DateTime::TimeZone::Local::Unix::_ReadEtcSysconfigClock = sub { 'US/Eastern' };

    my $tz;
    eval { $tz = DateTime::TimeZone::Local::Unix->FromEtcSysconfigClock() };
    is( $@, '', 'valid time zone name in /etc/sysconfig/clock should not die' );
    is( $tz->name(), 'America/New_York',
        'FromEtcSysConfigClock() with _ReadEtcSysconfigClock returning US/Eastern' );
}

SKIP:
{
    skip "cannot read /etc/default/init", 2
        unless -r '/etc/default/init' && -f _;

    local *DateTime::TimeZone::Local::Unix::_ReadEtcDefaultInit = sub { 'Asia/Tokyo' };

    my $tz;
    eval { $tz = DateTime::TimeZone::Local::Unix->FromEtcDefaultInit() };
    is( $@, '', 'valid time zone name in /etc/default/init should not die' );
    is( $tz->name(), 'Asia/Tokyo',
      'FromEtcDefaultInit with _ReadEtcDefaultInit returning Asia/Tokyo');
}

SKIP:
{
    skip "Cannot run these tests without explicitly knowing local time zone first (only runs on developers' machine)", 6
        unless $IsMaintainer;

    {
        local $ENV{TZ} = '';

        my $tz;
        eval { $tz = DateTime::TimeZone::Local->TimeZone() };
        is( $@, '', 'valid time zone name in /etc/localtime should not die' );
        is( $tz->name(), 'America/Chicago',
            '/etc/localtime should link to America/Chicago' );
    }

    {
        local *DateTime::TimeZone::Local::Unix::FromEtcLocaltime = sub { undef };

        my $tz;
        eval { $tz = DateTime::TimeZone::Local->TimeZone() };
        is( $@, '', 'valid time zone name in /etc/timezone should not die' );
        is( $tz->name(), 'America/Chicago',
            '/etc/timezone should contain America/Chicago' );
    }

    {
        # requires that /etc/default/init contain
        # TZ=Australia/Melbourne to work.
        local *DateTime::TimeZone::Local::Unix::FromEtcLocaltime = sub { undef };
        local *DateTime::TimeZone::Local::Unix::FromEtcTimezone = sub { undef };
        local *DateTime::TimeZone::Local::Unix::FromEtcTIMEZONE = sub { undef };

        my $tz;
        eval { $tz = DateTime::TimeZone::Local->TimeZone() };
        is( $@, '', '/etc/default/init contains TZ=Australia/Melbourne' );
        is( $tz->name(), 'Australia/Melbourne',
            '/etc/default/init should contain Australia/Melbourne' );
    }
}

SKIP:
{
    my $file = '/etc/timezone';

    skip "Cannot write this test unless we can write to /etc/timezone", 1
        unless $IsMaintainer && -w $file;

    open my $fh, '>', $file
        or die "Cannot write to $file: $!";
    print $fh 'Foo/Bar';
    close $fh;

    DateTime::TimeZone::Local::Unix->FromEtcTimezone();
    is( $@, '', 'calling FromEtcTimezone when it contains a bad name should not leave $@ set' );

    open $fh, '>', $file
        or die "Cannot write to $file: $!";
    print $fh 'America/Chicago';
    close $fh;
}

SKIP:
{
    my $file = '/etc/TIMEZONE';

    skip "Cannot write this test unless we can write to /etc/TIMEZONE", 1
        unless $IsMaintainer && -w '/etc';

    open my $fh, '>', $file
        or die "Cannot write to $file: $!";
    print $fh "TZ = Foo/Bar\n";
    close $fh;

    DateTime::TimeZone::Local::Unix->FromEtcTIMEZONE();
    is( $@, '', 'calling FromEtcTIMEZONE when it contains a bad name should not leave $@ set' );

    unlink $file
        or die "Cannot unlink $file: $!";
}

SKIP:
{
    skip "These tests are too dangerous to run on someone else's machine ;)", 5
        unless $IsMaintainer;

    skip "These tests can only be run if we can overwrite /etc/localtime", 5
        unless $CanWriteEtcLocaltime;

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
        eval { $tz = DateTime::TimeZone::Local->TimeZone() };
        is( $@, '', 'copy of zoneinfo file at /etc/localtime' );
        is( $tz->name(), 'Asia/Calcutta',
            '/etc/localtime should be a copy of Asia/Calcutta' );

        is( Cwd::cwd(), $cwd, 'cwd should not change after finding local time zone' );

        $tz = DateTime::TimeZone::Local->TimeZone();
        is( $@, '', 'calling _FindMatchZoneinfoFile does not leave $@ set' );
    }

    {
        local $ENV{TZ} = '';

        # Make sure that a die handler does not break our use of die
        # to escape from File::Find::find()
        local $SIG{__DIE__} = sub { die 'haha'; };

        my $tz;
        eval { $tz = DateTime::TimeZone::Local->TimeZone() };
        is( $tz->name(), 'Asia/Calcutta',
            'a __DIE__ handler did not interfere with our use of File::Find' );
    }

    unlink '/etc/localtime' or die "Cannot unlink /etc/localtime: $!";
    symlink $tz_file, '/etc/localtime'
        or die "Cannot symlink $tz_file to '/etc/localtime': $!";
}

{
    local $ENV{TZ} = 'Australia/Melbourne';
    my $tz = eval { DateTime::TimeZone->new( name => 'local' ) };
    is( $tz->name(), 'Australia/Melbourne',
        q|DT::TZ->new( name => 'local' )| );
}

SKIP:
{
    skip "These tests require File::Temp", 1
        unless require File::Temp;
    skip "These tests require a filesystem which support symlinks", 1
        unless eval { symlink '', '' ; 1 };

    my $tempdir = File::Temp::tempdir( CLEANUP => 1 );

    my $first = File::Spec->catfile( $tempdir, 'first' );
    open my $fh, '>', $first
        or die "Cannot open $first: $!";
    close $fh;

    my $second = File::Spec->catfile( $tempdir, 'second' );
    symlink $first => $second
        or die "Cannot symlink $first => $second: $!";

    my $third = File::Spec->catfile( $tempdir, 'third' );
    symlink $second => $third
        or die "Cannot symlink $first => $second: $!";

    # It seems that on some systems (OSX, others?) the temp directory
    # returned by File::Temp may be a symlink (/tmp is a link to
    # /private/tmp), so when abs_path folows that link, we end up with
    # a different path to the "first" file.
    is( basename( DateTime::TimeZone::Local::Unix->_Readlink( $third ) ),
        basename( $first ),
        '_Readlink follows multiple levels of symlinking' );
}
