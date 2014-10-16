use Test::More;
use HTTP::Proxy::Engine;

plan tests => 18;

my $e;
my $p = bless {}, "HTTP::Proxy";

$e = HTTP::Proxy::Engine->new( proxy => $p, engine => Legacy );
isa_ok( $e, 'HTTP::Proxy::Engine::Legacy' );

# use the default engine for $^O
eval { HTTP::Proxy::Engine->new() };
isa_ok( $e, 'HTTP::Proxy::Engine' );

eval { HTTP::Proxy::Engine->new( engine => Legacy ) };
like( $@, qr/^No proxy defined/, "proxy required" );

eval { HTTP::Proxy::Engine->new( proxy => "P", engine => Legacy ) };
like( $@, qr/^P is not a HTTP::Proxy object/, "REAL proxy required" );

# direct engine creation
# HTTP::Proxy::Engine::Legacy was required before
$e = HTTP::Proxy::Engine::Legacy->new( proxy => $p );
isa_ok( $e, 'HTTP::Proxy::Engine::Legacy' );

eval { HTTP::Proxy::Engine::Legacy->new() };
like( $@, qr/^No proxy defined/, "proxy required" );

eval { HTTP::Proxy::Engine::Legacy->new( proxy => "P" ) };
like( $@, qr/^P is not a HTTP::Proxy object/, "REAL proxy required" );

# non-existent engine
eval { HTTP::Proxy::Engine->new( proxy => $p, engine => Bonk ) };
like(
    $@,
    qr/^Can't locate HTTP.+?Proxy.+?Engine.+?Bonk\.pm in \@INC/,
    "Engine Bonk does not exist"
);

# check the base accessor
$e = HTTP::Proxy::Engine->new( proxy => $p, engine => Legacy );
is( $e->proxy, $p, "proxy() get" );

# check subclasses accessors
$e = HTTP::Proxy::Engine->new( proxy => $p, engine => Legacy, select => 2 );
is( $e->select,    2, "subclass get()" );
is( $e->select(4), 4, "subclass set()" );
is( $e->select,    4, "subclass get()" );

$e = HTTP::Proxy::Engine::Legacy->new( proxy => $p, select => 3 );
is( $e->select,    3, "subclass get()" );
is( $e->select(4), 4, "subclass set()" );
is( $e->select,    4, "subclass get()" );

# but where is the code?
is( *{HTTP::Proxy::Engine::select}{CODE},
    undef, "code not in the base class" );
is( ref *{HTTP::Proxy::Engine::select}{CODE},
    '', "code not in the base class" );
my $c = \&HTTP::Proxy::Engine::Legacy::select; # remove "used only once" warning
is( ref *{HTTP::Proxy::Engine::Legacy::select}{CODE},
    'CODE', "code in the subclass" );
