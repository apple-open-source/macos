use strict;
use warnings;

use File::Spec;
use Test::More;

use lib File::Spec->catdir( File::Spec->curdir, 't' );

BEGIN
{
    require 'check_datetime_version.pl';

    eval { require Test::Output };
    if ($@)
    {
        plan skip_all => 'These tests require Test::Output.';
    }
}

plan tests => 2;


{
    Test::Output::stderr_like
        ( sub { DateTime::TimeZone->new( name => 'Fake/TZ' ) },
          qr/\Qfrom an older version (unknown)/,
          'loading timezone where olson version is not defined'
        );
}

{
    Test::Output::stderr_like
        ( sub { DateTime::TimeZone->new( name => 'Fake/TZ2' ) },
          qr/\Qfrom an older version (2000a)/,
          'loading timezone where olson version is older than current'
        );
}


package DateTime::TimeZone::Fake::TZ;

use strict;

use Class::Singleton;
use DateTime::TimeZone;
use DateTime::TimeZone::OlsonDB;

use base 'Class::Singleton', 'DateTime::TimeZone';

sub is_olson { 1 }


package DateTime::TimeZone::Fake::TZ2;

use strict;

use Class::Singleton;
use DateTime::TimeZone;
use DateTime::TimeZone::OlsonDB;

use base 'Class::Singleton', 'DateTime::TimeZone';

sub is_olson { 1 }

sub olson_version { '2000a' }
