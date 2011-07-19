use warnings;
use strict;

use Test::More;
use lib qw(t/lib);
use DBICTest;

# Don't run tests for installs
unless ( DBICTest::AuthorCheck->is_author || $ENV{AUTOMATED_TESTING} || $ENV{RELEASE_TESTING} ) {
  plan( skip_all => "Author tests not required for installation" );
}

require DBIx::Class;
unless ( DBIx::Class::Optional::Dependencies->req_ok_for ('test_pod') ) {
  my $missing = DBIx::Class::Optional::Dependencies->req_missing_for ('test_pod');
  $ENV{RELEASE_TESTING} || DBICTest::AuthorCheck->is_author
    ? die ("Failed to load release-testing module requirements: $missing")
    : plan skip_all => "Test needs: $missing"
}

Test::Pod::all_pod_files_ok();
