#!perl -T

use strict;
use warnings;

use Test::More tests => 1;

BEGIN {
	use_ok( 'Variable::Magic' );
}

my $p = Variable::Magic::VMG_PERL_PATCHLEVEL;
$p = $p ? 'patchlevel ' . $p : 'no patchlevel';
diag( "Testing Variable::Magic $Variable::Magic::VERSION, Perl $] ($p), $^X" );

if (eval { require ActivePerl; 1 } and defined &ActivePerl::BUILD) {
 diag "This is ActiveState Perl $] build " . ActivePerl::BUILD();
}
