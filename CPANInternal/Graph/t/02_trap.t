use Test::More tests => 2;

use Graph;

isnt($SIG{__DIE__},  \&Graph::__carp_confess, '$SIG{__DIE__}' );
isnt($SIG{__WARN__}, \&Graph::__carp_confess, '$SIG{__WARN__}');




