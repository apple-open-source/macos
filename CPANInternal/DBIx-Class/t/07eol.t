use warnings;
use strict;

use Test::More;
use lib 't/lib';
use DBICTest;

my @MODULES = (
  'Test::EOL 0.6',
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

TODO: {
  local $TODO = 'Do not fix those yet - we have way too many branches out there, merging will be hell';
  all_perl_files_ok({ trailing_whitespace => 1}, qw/t lib script maint/);
}

done_testing;
