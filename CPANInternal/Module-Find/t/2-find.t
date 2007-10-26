use Test::More tests => 5;

use Module::Find;

use lib qw(./test);

my @l;

@l = findsubmod ModuleFindTest;

ok($#l == 0);
ok($l[0] eq 'ModuleFindTest::SubMod');

@l = findallmod ModuleFindTest;

ok($#l == 1);
ok($l[0] eq 'ModuleFindTest::SubMod');
ok($l[1] eq 'ModuleFindTest::SubMod::SubSubMod');




