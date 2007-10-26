use Test::More tests => 5;

use Module::Find;

use lib qw(./test);

my @l;

@l = useall ModuleFindTest;

ok($#l == 1);
ok($l[0] eq 'ModuleFindTest::SubMod');
ok($l[1] eq 'ModuleFindTest::SubMod::SubSubMod');
ok($ModuleFindTest::SubMod::loaded);
ok($ModuleFindTest::SubMod::SubSubMod::loaded);

package ModuleFindTest::SubMod;

$ModuleFindTest::SubMod::loaded = 0;

package ModuleFindTest::SubSubMod;

$ModuleFindTest::SubMod::SubSubMod::loaded = 0;




