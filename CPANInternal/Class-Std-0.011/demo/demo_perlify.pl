package Base1;
use Class::Std;

my %foo : ATTR( :init_arg<foo> );
my %bar : ATTR( :init_arg<bar> );
 
sub foo {}

package Base2;
use Class::Std;

my %baz : ATTR( :set<baz> );

sub foo {}

package Der;
use base qw( Base1 Base2 );


package Other;
use Class::Std;

my %qux : ATTR( :set<qux> );


package main;

my $obj = Der->new({foo=>'foo val', bar=>42});
print $obj->_DUMP();

my $obj2 = Other->new();
print $obj2->_DUMP();

