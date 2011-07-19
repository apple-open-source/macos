use strict;
use warnings;

use Test::More tests => 3;
use Test::NoWarnings;

use Test::Deep::NoTest;

ok(eq_deeply([], []), "got eq_deeply");
ok(! eq_deeply({}, []), "eq_deeply works");
