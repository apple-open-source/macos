use Test::More tests => 1;

use lib 't';
use MyUGraph;

# http://rt.cpan.org/NoAuth/Bug.html?id=6429
ok(ref(new UGraph), "UGraph");
