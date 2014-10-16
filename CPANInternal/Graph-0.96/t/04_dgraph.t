use Test::More tests => 1;

use lib 't';
use MyDGraph;

# http://rt.cpan.org/NoAuth/Bug.html?id=6429
ok(ref(new DGraph), "DGraph");
