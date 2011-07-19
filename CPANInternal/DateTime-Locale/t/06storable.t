use strict;
use warnings;

use Test::More;

BEGIN {
    eval { require Storable };

    if ($@) {
        plan skip_all => 'These tests require the Storable mdoule';
    }
    else {
        plan tests => 3;
    }
}

use DateTime::Locale;

use Storable;

my $loc1   = DateTime::Locale->load('en_US');
my $frozen = Storable::nfreeze($loc1);

ok(
    length $frozen < 2000,
    'the serialized locale object should not be immense'
);

my $loc2 = Storable::thaw($frozen);

is( $loc2->id, 'en_US', 'thaw frozen locale object' );

my $loc3 = Storable::dclone($loc1);

is( $loc3->id, 'en_US', 'dclone object' );
