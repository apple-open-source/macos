#!perl

use Test::More tests => 8;
use Sub::Identify ':all';

ok( !defined sub_name( undef ) );
ok( !defined sub_name( "scalar" ) );
ok( !defined sub_name( \"scalar ref" ) );
ok( !defined sub_name( \@INC ) );

ok( !defined stash_name( undef ) );
ok( !defined stash_name( "scalar" ) );
ok( !defined stash_name( \"scalar ref" ) );
ok( !defined stash_name( \@INC ) );
