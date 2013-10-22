package dbixcsl_test_dir;

use strict;
use warnings;
use File::Path 'rmtree';
use Scalar::Util 'weaken';
use namespace::clean;
use DBI ();

our $tdir = 't/var';

use base qw/Exporter/;
our @EXPORT_OK = '$tdir';

die "/t does not exist, this can't be right...\n"
  unless -d 't';

unless (-d $tdir) {
  mkdir $tdir or die "Unable to create $tdir: $!\n";
}

# We need to disconnect all active DBI handles before deleting the directory,
# otherwise the SQLite .db files cannot be deleted on Win32 (file in use) since
# END does not run in any sort of order.

no warnings 'redefine';

my $connect = \&DBI::connect;

my @handles;

*DBI::connect = sub {
    my $dbh = $connect->(@_);
    push @handles, $dbh;
    weaken $handles[-1];
    return $dbh;
};

END {
    if (not $ENV{SCHEMA_LOADER_TESTS_NOCLEANUP}) {
        foreach my $dbh (@handles) {
            $dbh->disconnect if $dbh;
        }

        rmtree($tdir, 1, 1)
    }
}

1;
