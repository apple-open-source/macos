use strict;
use warnings;
use File::Spec;
use Test::More;
require Test::Perl::Critic;

my $rcfile = File::Spec->catfile( 'xt', 'perlcriticrc' );
Test::Perl::Critic->import( -profile => $rcfile );
all_critic_ok( 'lib', 'examples'  );
