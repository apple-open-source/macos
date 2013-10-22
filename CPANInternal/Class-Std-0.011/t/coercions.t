my  %global_hash;
my  @global_array;
my  $global_scalar;
sub  global_sub {}

my  %global_hash2;
my  @global_array2;
my  $global_scalar2;
sub  global_sub2 {}

# Test basic coercions...
package BaseClass;
use Class::Std;
{
    sub as_str    : STRINGIFY  { return 'hello world' }
    sub as_num    : NUMERIFY   { return 42 }
    sub as_bool   : BOOLIFY    { return }

    sub as_code   : CODIFY     { return \&::global_sub    }
    sub as_glob   : GLOBIFY    { return \*::global_glob   }
    sub as_hash   : HASHIFY    { return \%global_hash   }
    sub as_array  : ARRAYIFY   { return \@global_array  }
    sub as_scalar : SCALARIFY  { return \$global_scalar }
}

# Test inheritance without change...
package DerClass;
use base qw( BaseClass );

# Test inheritance with change...
package DerClass2;
use Class::Std;
use base qw( BaseClass );
{
    sub as_str    : STRINGIFY  { return 'goodbye world' }
    sub as_num    : NUMERIFY   { return 86 }
    sub as_bool   : BOOLIFY    { return 1 }

    sub as_code   : CODIFY     { return \&::global_sub2    }
    sub as_glob   : GLOBIFY    { return \*::global_glob2   }
    sub as_hash   : HASHIFY    { return \%global_hash2     }
    sub as_array  : ARRAYIFY   { return \@global_array2    }
    sub as_scalar : SCALARIFY  { return \$global_scalar2   }
}

# Test inheritance with change and they didn't "use Class::Std"
package DerClass3;
use base qw( BaseClass );
{
    sub as_str    : STRINGIFY  { return 'goodbye world' }
    sub as_num    : NUMERIFY   { return 86 }
    sub as_bool   : BOOLIFY    { return 1 }

    sub as_code   : CODIFY     { return \&::global_sub2    }
    sub as_glob   : GLOBIFY    { return \*::global_glob2   }
    sub as_hash   : HASHIFY    { return \%global_hash2     }
    sub as_array  : ARRAYIFY   { return \@global_array2    }
    sub as_scalar : SCALARIFY  { return \$global_scalar2   }
}

# Test inheritance with change and they don't re-specify the coercions
package DerClass4;
use base qw( BaseClass );
{
    sub as_str     { return 'goodbye world' }
    sub as_num     { return 86 }
    sub as_bool    { return 1 }

    sub as_code    { return \&::global_sub2    }
    sub as_glob    { return \*::global_glob2   }
    sub as_hash    { return \%global_hash2     }
    sub as_array   { return \@global_array2    }
    sub as_scalar  { return \$global_scalar2   }
}

# Test inheritance with changing the subs used for the coercions
package DerClass5;
use base qw( BaseClass );
{
    sub as_str_changed    : STRINGIFY { return 'goodbye world' }
    sub as_num_changed    : NUMERIFY  { return 86 }
    sub as_bool_changed   : BOOLIFY   { return 1 }

    sub as_code_changed   : CODIFY    { return \&::global_sub2    }
    sub as_glob_changed   : GLOBIFY   { return \*::global_glob2   }
    sub as_hash_changed   : HASHIFY   { return \%global_hash2     }
    sub as_array_changed  : ARRAYIFY  { return \@global_array2    }
    sub as_scalar_changed : SCALARIFY { return \$global_scalar2   }
}


package main;

use Test::More 'no_plan';

my $obj;

# Basic coercions...

$obj = BaseClass->new();

ok !$obj                            => 'Base Boolean coercion';
is 0+$obj, 42                       => 'Base Numeric coercion';
is "$obj", 'hello world'            => 'Base String coercion';

is \&{$obj}, \&global_sub           => 'Base Code coercion';
is \*{$obj}, \*global_glob          => 'Base Glob coercion';
is \%{$obj}, \%global_hash          => 'Base Hash coercion';
is \@{$obj}, \@global_array         => 'Base Array coercion';
is \${$obj}, \$global_scalar        => 'Base Scalar coercion';


# Inheriting coercions...

$obj = DerClass->new();

ok !$obj                            => 'Der Boolean coercion';
is 0+$obj, 42                       => 'Der Numeric coercion';
is "$obj", 'hello world'            => 'Der String coercion';

is \&{$obj}, \&global_sub           => 'Der Code coercion';
is \*{$obj}, \*global_glob          => 'Der Glob coercion';
is \%{$obj}, \%global_hash          => 'Der Hash coercion';
is \@{$obj}, \@global_array         => 'Der Array coercion';
is \${$obj}, \$global_scalar        => 'Der Scalar coercion';


# Redefining coercions on inheritance...

$obj = DerClass2->new();

ok $obj                             => 'Der2 Boolean coercion';
is 0+$obj, 86                       => 'Der2 Numeric coercion';
is "$obj", 'goodbye world'          => 'Der2 String coercion';

is \&{$obj}, \&global_sub2          => 'Der2 Code coercion';
is \*{$obj}, \*global_glob2         => 'Der2 Glob coercion';
is \%{$obj}, \%global_hash2         => 'Der2 Hash coercion';
is \@{$obj}, \@global_array2        => 'Der2 Array coercion';
is \${$obj}, \$global_scalar2       => 'Der2 Scalar coercion';


# Redefining coercions on inheritance and there is no "use Class::Std" 
# in the subclass

$obj = DerClass3->new();

ok $obj                             => 'Der3 Boolean coercion';
is 0+$obj, 86                       => 'Der3 Numeric coercion';
is "$obj", 'goodbye world'          => 'Der3 String coercion';

is \&{$obj}, \&global_sub2          => 'Der3 Code coercion';
is \*{$obj}, \*global_glob2         => 'Der3 Glob coercion';
is \%{$obj}, \%global_hash2         => 'Der3 Hash coercion';
is \@{$obj}, \@global_array2        => 'Der3 Array coercion';
is \${$obj}, \$global_scalar2       => 'Der3 Scalar coercion';


# The subclass doesn't need to specify the coercions again

$obj = DerClass4->new();

ok $obj                             => 'Der4 Boolean coercion';
is 0+$obj, 86                       => 'Der4 Numeric coercion';
is "$obj", 'goodbye world'          => 'Der4 String coercion';

is \&{$obj}, \&global_sub2          => 'Der4 Code coercion';
is \*{$obj}, \*global_glob2         => 'Der4 Glob coercion';
is \%{$obj}, \%global_hash2         => 'Der4 Hash coercion';
is \@{$obj}, \@global_array2        => 'Der4 Array coercion';
is \${$obj}, \$global_scalar2       => 'Der4 Scalar coercion';


# The subclass doesn't need to specify the coercions again

$obj = DerClass5->new();

ok $obj                             => 'Der5 Boolean coercion';
is 0+$obj, 86                       => 'Der5 Numeric coercion';
is "$obj", 'goodbye world'          => 'Der5 String coercion';

is \&{$obj}, \&global_sub2          => 'Der5 Code coercion';
is \*{$obj}, \*global_glob2         => 'Der5 Glob coercion';
is \%{$obj}, \%global_hash2         => 'Der5 Hash coercion';
is \@{$obj}, \@global_array2        => 'Der5 Array coercion';
is \${$obj}, \$global_scalar2       => 'Der5 Scalar coercion';

