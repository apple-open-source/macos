use Test::More 'no_plan';

package Problem;
use Class::Std;

# overload seems to interfere with overloading coercions
sub as_string : STRINGIFY { return 'string'; }

package main;

our $obj;
BEGIN { 
    $obj = Problem->new();
    ok("$obj");
}

ok("$obj");

