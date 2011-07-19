use strict;
use warnings;
use Test::More tests => 12;
use Context::Preserve;
use Test::Exception;
my $after = 0;

is $after, 0;
is_deeply [foo()], [qw/an array/];
is $after, 1;
$after = 0;
is scalar foo(), 'scalar';
is $after, 1;

is_deeply [bar()], [qw/an42 array42/];
is scalar bar(), 'scalar42';

is_deeply [baz()], [qw/anARRAY arrayARRAY/];
is scalar baz(), 'scalarSCALAR';

is_deeply [quux()], [qw/hello there friendly world/];
is scalar quux(), 'world';

throws_ok { preserve_context {}, made_up => sub {} }
  qr/need an "after" or "replace" coderef/;

sub code {
    if(wantarray){ 
        return qw/an array/ 
    } 
    else { 
        return 'scalar' 
    }
};

sub foo {
    return preserve_context {
        return code();
    } after => sub { $after = 1 };
}

sub bar {
    return preserve_context {
        return code();
    } after => sub { $_ .= "42" for @_ };
}

sub baz {
    return preserve_context {
        return code();
    } after => sub { 
        my $wa = wantarray ? "ARRAY" : "SCALAR";
        $_ .= "$wa" for @_ ;
        return qw/oh noes/; # this is ignored
    };   
}

# this was a good idea when i had one function, now it's getting old
sub quux {
    return preserve_context {
        return code();
    } replace => sub { 
        return qw/hello there friendly world/;
    };
}
