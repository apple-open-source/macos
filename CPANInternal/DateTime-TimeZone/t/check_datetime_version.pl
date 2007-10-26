use strict;

use DateTime::TimeZone;

BEGIN
{
    my $version = '0.1501';
    eval "use DateTime $version";
    if ($@)
    {
        Test::More::plan
            ( skip_all => "Cannot run tests before DateTime.pm $version is installed." );
        exit;
    }
}

1;
