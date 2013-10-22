use strict;
use Test::More;

#----------------------------------------------------------------------
# Test lazy loading
#----------------------------------------------------------------------

BEGIN {
	eval "use DBD::SQLite";
	plan $@ ? (skip_all => 'needs DBD::SQLite for testing') : (tests => 25);
}

INIT {
	use lib 't/testlib';
	use Lazy;
}

is_deeply [ Lazy->columns('Primary') ],        [qw/this/],      "Pri";
is_deeply [ sort Lazy->columns('Essential') ], [qw/opop this/], "Essential";
is_deeply [ sort Lazy->columns('things') ],    [qw/that this/], "things";
is_deeply [ sort Lazy->columns('horizon') ],   [qw/eep orp/],   "horizon";
is_deeply [ sort Lazy->columns('vertical') ],  [qw/oop opop/],  "vertical";
is_deeply [ sort Lazy->columns('All') ], [qw/eep oop opop orp that this/],
	"All";

{
	my @groups = Lazy->__grouper->groups_for(Lazy->find_column('this'));
	is_deeply [ sort @groups ], [qw/Essential Primary things/],
		"this (@groups)";
}

{
	my @groups = Lazy->__grouper->groups_for(Lazy->find_column('that'));
	is_deeply [@groups], [qw/things/], "that (@groups)";

}

Lazy->insert({ this => 1, that => 2, oop => 3, opop => 4, eep => 5 });

ok(my $obj = Lazy->retrieve(1), 'Retrieve by Primary');
ok($obj->_attribute_exists('this'),  "Gets primary");
ok($obj->_attribute_exists('opop'),  "Gets other essential");
ok(!$obj->_attribute_exists('that'), "But other things");
ok(!$obj->_attribute_exists('eep'),  " nor eep");
ok(!$obj->_attribute_exists('orp'),  " nor orp");
ok(!$obj->_attribute_exists('oop'),  " nor oop");

ok(my $val = $obj->eep, 'Fetch eep');
ok($obj->_attribute_exists('orp'),   'Gets orp too');
ok(!$obj->_attribute_exists('oop'),  'But still not oop');
ok(!$obj->_attribute_exists('that'), 'nor that');

{
	Lazy->columns(All => qw/this that eep orp oop opop/);
	ok(my $obj = Lazy->retrieve(1), 'Retrieve by Primary');
	ok !$obj->_attribute_exists('oop'), " Don't have oop";
	my $null = $obj->eep;
	ok !$obj->_attribute_exists('oop'),
		" Don't have oop - even after getting eep";
}

# Test contructor breaking.

eval {    # Need a hashref
	Lazy->insert(this => 10, that => 20, oop => 30, opop => 40, eep => 50);
};
ok($@, $@);

eval {    # False column
	Lazy->insert({ this => 10, that => 20, theother => 30 });
};
ok($@, $@);

eval {    # Multiple false columns
	Lazy->insert({ this => 10, that => 20, theother => 30, andanother => 40 });
};
ok($@, $@);

