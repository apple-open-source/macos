use warnings;
use strict;

use Test::More;
use lib 't/lib';
use DBICTest;

my @MODULES = (
  'Test::NoTabs 0.9',
);

plan skip_all => 'Does not work with done_testing, temp disabled';

# Don't run tests for installs
unless ( DBICTest::AuthorCheck->is_author || $ENV{AUTOMATED_TESTING} || $ENV{RELEASE_TESTING} ) {
  plan( skip_all => "Author tests not required for installation" );
}
# Load the testing modules
foreach my $MODULE ( @MODULES ) {
  eval "use $MODULE";
  if ( $@ ) {
    $ENV{RELEASE_TESTING}
    ? die( "Failed to load required release-testing module $MODULE" )
    : plan( skip_all => "$MODULE not available for testing" );
  }
}

all_perl_files_ok(qw/t lib script maint/);

done_testing;
