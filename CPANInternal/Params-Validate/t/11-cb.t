use strict;
use warnings;

use File::Spec;
use lib File::Spec->catdir( 't', 'lib' );

use PVTests::Callbacks;
PVTests::Callbacks::run_tests();
