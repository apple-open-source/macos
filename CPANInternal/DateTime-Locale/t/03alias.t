use strict;
use warnings;

use Test::More tests => 5;

use DateTime::Locale;

DateTime::Locale->add_aliases( foo => 'root' );
DateTime::Locale->add_aliases( bar => 'foo' );
DateTime::Locale->add_aliases( baz => 'bar' );
eval { DateTime::Locale->add_aliases( bar => 'baz' ) };

like( $@, qr/loop/, 'cannot add an alias that would cause a loop' );

my $l = DateTime::Locale->load('baz');
isa_ok( $l, 'DateTime::Locale::Base' );
is( $l->id, 'baz', 'id is baz' );

ok( DateTime::Locale->remove_alias('baz'),
    'remove_alias should return true' );

eval { DateTime::Locale->load('baz') };
like( $@, qr/invalid/i, 'removed alias should be gone' );
