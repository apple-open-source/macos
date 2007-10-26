use Test::More tests => 9;

use Module::Find;

# First, with @INC only

@l = findsubmod ModuleFindTest;
ok($#l == -1);

@l = findallmod ModuleFindTest;
ok($#l == -1);

# Then, including our directory

setmoduledirs('./test');

@l = findsubmod ModuleFindTest;
ok($#l == 0);
ok($l[0] eq 'ModuleFindTest::SubMod');

@l = findallmod ModuleFindTest;
ok($#l == 1);
ok($l[0] eq 'ModuleFindTest::SubMod');
ok($l[1] eq 'ModuleFindTest::SubMod::SubSubMod');

# Third, reset to @INC only

setmoduledirs();

@l = findsubmod ModuleFindTest;
ok($#l == -1);

@l = findallmod ModuleFindTest;
ok($#l == -1);

# Then, including our directory


