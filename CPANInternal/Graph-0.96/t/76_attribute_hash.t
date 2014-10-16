use Test::More tests => 9;
package Hash;
use Graph::Attribute hash  => _A;
sub new { bless {}, shift }
package main;
my $o = Hash->new();
ok(!$o->_has_attributes());
is(my $a = $o->_get_attributes(), undef);
ok($o->_set_attributes({foo => 42}));
ok($o->_has_attributes());
ok($a = $o->_get_attributes());
is($a->{foo}, 42);
ok($o->_delete_attributes());
ok(!$o->_has_attributes());
is($a = $o->_get_attributes(), undef);
