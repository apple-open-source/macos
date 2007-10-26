use Test::More tests => 4;

use Module::Find;

use lib qw(./test);

my @l;

@l = usesub ModuleFindTest;

ok($#l == 0);
ok($l[0] eq 'ModuleFindTest::SubMod');
ok($ModuleFindTest::SubMod::loaded);
ok(!$ModuleFindTest::SubMod::SubSubMod::loaded);

package ModuleFindTest::SubMod;

$ModuleFindTest::SubMod::loaded = 0;

package ModuleFindTest::SubSubMod;

$ModuleFindTest::SubMod::SubSubMod::loaded = 0;


