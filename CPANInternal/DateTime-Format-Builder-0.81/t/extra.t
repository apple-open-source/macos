use strict;
use warnings;

use Test::More;

use DateTime::Format::Builder;

{
    my $parser = DateTime::Format::Builder->parser(
        {
            params => [qw( year month day hour minute second )],
            regex  => qr/^(\d\d\d\d)(\d\d)(\d\d)T(\d\d)(\d\d)(\d\d)$/,
            extra  => { time_zone => 'America/Chicago' },
        }
    );

    my $dt = $parser->parse_datetime("20030716T163245");

    is( $dt->time_zone->name, 'America/Chicago' );
}

done_testing();
