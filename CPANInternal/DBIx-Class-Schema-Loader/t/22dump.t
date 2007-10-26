use strict;
use Test::More;
use lib qw(t/lib);
use File::Path;
use make_dbictest_db;

my $dump_path = './t/_dump';

{
    package DBICTest::Schema::1;
    use base qw/ DBIx::Class::Schema::Loader /;
    __PACKAGE__->loader_options(
        relationships => 1,
        dump_directory => $dump_path,
    );
}

{
    package DBICTest::Schema::2;
    use base qw/ DBIx::Class::Schema::Loader /;
    __PACKAGE__->loader_options(
        relationships => 1,
        dump_directory => $dump_path,
        dump_overwrite => 1,
    );
}

plan tests => 8;

rmtree($dump_path, 1, 1);

eval { DBICTest::Schema::1->connect($make_dbictest_db::dsn) };
ok(!$@, 'no death with dump_directory set') or diag "Dump failed: $@";

DBICTest::Schema::1->loader(undef);

SKIP: {
  skip "ActiveState perl produces additional warnings", 5
    if ($^O eq 'MSWin32');

  my @warn_output;
  {
      local $SIG{__WARN__} = sub { push(@warn_output, @_) };
      DBICTest::Schema::1->connect($make_dbictest_db::dsn);
  }
  my @warnings_regexes = (
      qr|Dumping manual schema|,
      (qr|DBICTest/Schema/1.*?.pm exists, will not overwrite|) x 3,
      qr|Schema dump completed|,
  );

  like(shift @warn_output, $_) foreach (@warnings_regexes);

  rmtree($dump_path, 1, 1);
}

eval { DBICTest::Schema::2->connect($make_dbictest_db::dsn) };
ok(!$@, 'no death with dump_directory set (overwrite1)')
    or diag "Dump failed: $@";

DBICTest::Schema::2->loader(undef);
eval { DBICTest::Schema::2->connect($make_dbictest_db::dsn) };
ok(!$@, 'no death with dump_directory set (overwrite2)')
    or diag "Dump failed: $@";

END { rmtree($dump_path, 1, 1); }
