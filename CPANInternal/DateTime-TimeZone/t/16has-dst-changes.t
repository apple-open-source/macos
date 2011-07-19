use strict;
use warnings;

use File::Spec;
use Test::More;

use lib File::Spec->catdir( File::Spec->curdir, 't' );

BEGIN { require 'check_datetime_version.pl' }

plan tests => 4;

ok( ! DateTime::TimeZone->new( name => 'UTC' )->has_dst_changes,
    'UTC has no DST changes' );
ok( ! DateTime::TimeZone->new( name => 'floating' )->has_dst_changes,
    'floating has no DST changes' );
ok( ! DateTime::TimeZone->new( name => 'Asia/Thimphu' )->has_dst_changes,
    'Asia/Thimphu has no DST changes' );
ok( DateTime::TimeZone->new( name => 'America/Chicago' )->has_dst_changes,
    'America/chicago has DST changes' );
