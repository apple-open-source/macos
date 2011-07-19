use Test::More tests => 1;

use Module::Find;

use lib qw(./test ./test/duplicates);

# Ensure duplicate modules are only reported once
my @l = useall ModuleFindTest;
ok($#l == 1);