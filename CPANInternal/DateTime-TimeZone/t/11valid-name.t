use strict;
use warnings;

use File::Spec;
use Test::More;

use lib File::Spec->catdir( File::Spec->curdir, 't' );

BEGIN { require 'check_datetime_version.pl' }

plan tests => 13;

foreach ( qw( America/Chicago
              UTC
              US/Eastern
              Europe/Paris
              Etc/Zulu
              Pacific/Midway
              EST
            ) )
{
    ok( DateTime::TimeZone->is_valid_name($_),
        "$_ is a valid timezone name" );
}

foreach ( qw( America/Hell
              Foo/Bar
              FooBar
              adhdsjghs;dgohas098huqjy4ily
              1000:0001
            ) )
{
    ok( ! DateTime::TimeZone->is_valid_name($_),
        "$_ is not a valid timezone name" );
}

{
    DateTime::TimeZone->is_valid_name(undef);

    is( $@, '',
        'calling is_valid_name with a bad argument does not leave $@ set' );
}

