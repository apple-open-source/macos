package Foo;

# Hook::LexWrap does this, Sub::Uplevel appears to interfere.
sub import { *{caller()."::fooble"} = \&fooble }

sub fooble { 42 }

1;
