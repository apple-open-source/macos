use strict;
use warnings;

use Test::More;

unless ( -d '.git' ) {
    plan skip_all => 'This test only runs for the maintainer';
    exit;
}


system( $^X, 't/email-exit-helper.pl' );

is( $? >> 8, 5, 'exit code of helper was 5' );

done_testing();
