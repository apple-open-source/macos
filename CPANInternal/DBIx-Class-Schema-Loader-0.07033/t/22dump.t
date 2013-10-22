use strict;
use Test::More;
use Test::Exception;
use Test::Warn;
use lib qw(t/lib);
use File::Path;
use make_dbictest_db;
use dbixcsl_test_dir qw/$tdir/;

my $dump_path = "$tdir/dump";


{
    package DBICTest::Schema::1;
    use base qw/ DBIx::Class::Schema::Loader /;
    __PACKAGE__->loader_options(
        dump_directory => $dump_path,
    );
}

{
    package DBICTest::Schema::2;
    use base qw/ DBIx::Class::Schema::Loader /;
    __PACKAGE__->loader_options(
        dump_directory => $dump_path,
        really_erase_my_files => 1,
    );
}

plan tests => 7;

rmtree($dump_path, 1, 1);

lives_ok {
  warnings_exist { DBICTest::Schema::1->connect($make_dbictest_db::dsn) }
    [ qr|^Dumping manual schema|, qr|^Schema dump completed| ];
} 'no death with dump_directory set' or diag "Dump failed: $@";

DBICTest::Schema::1->_loader_invoked(undef);

SKIP: {
  skip "ActiveState perl produces additional warnings", 1
    if ($^O eq 'MSWin32');

  warnings_exist { DBICTest::Schema::1->connect($make_dbictest_db::dsn) }
    [ qr|^Dumping manual schema|, qr|^Schema dump completed| ];

  rmtree($dump_path, 1, 1);
}

lives_ok {
  warnings_exist { DBICTest::Schema::2->connect($make_dbictest_db::dsn) }
    [ qr|^Dumping manual schema|, qr|^Schema dump completed| ];
} 'no death with dump_directory set (overwrite1)' or diag "Dump failed: $@";

DBICTest::Schema::2->_loader_invoked(undef);

lives_ok {
  warnings_exist { DBICTest::Schema::2->connect($make_dbictest_db::dsn) }
  [
    qr/^Dumping manual schema/,
    qr|^Deleting .+Schema/2.+ due to 'really_erase_my_files'|,
    qr|^Deleting .+Schema/2/Result/Foo.+ due to 'really_erase_my_files'|,
    qr|^Deleting .+Schema/2/Result/Bar.+ due to 'really_erase_my_files'|,
    qr/^Schema dump completed/
  ];
} 'no death with dump_directory set (overwrite2)' or diag "Dump failed: $@";

END { rmtree($dump_path, 1, 1); }
