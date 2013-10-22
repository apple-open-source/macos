package Foo;
use Class::Std;

sub as_str   : STRINGIFY { return 'foo' }
sub as_num   : NUMERIFY  { return 42 }
sub as_bool  : BOOLIFY   { return 1 }
sub as_hash  : HASHIFY   { return {key=>'value'} }
sub as_array : ARRAYIFY  { return [99..101] }
sub as_code  : CODIFY    { sub { return 'code' } }
sub as_glob  : GLOBIFY   { local *FOO; return \*FOO }


package main;

my $obj = Foo->new();

use Smart::Comments;

### "$obj"
### 0+$obj

my $bool = $obj?"true\n":"false\n";
### $bool

### $obj->{key}

### $obj->[1]

### $obj->()

### *{$obj}

