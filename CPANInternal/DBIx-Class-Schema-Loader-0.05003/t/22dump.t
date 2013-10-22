use strict;
use Test::More;
use lib qw(t/lib);
use File::Path;
use make_dbictest_db;

my $dump_path = './t/_dump';

local $SIG{__WARN__} = sub {
    warn $_[0] unless $_[0] =~
        /really_erase_my_files|Dumping manual schema|Schema dump completed/;
};

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

plan tests => 5;

rmtree($dump_path, 1, 1);

eval { DBICTest::Schema::1->connect($make_dbictest_db::dsn) };
ok(!$@, 'no death with dump_directory set') or diag "Dump failed: $@";

DBICTest::Schema::1->_loader_invoked(undef);

SKIP: {
  my @warnings_regexes = (
      qr|Dumping manual schema|,
      qr|Schema dump completed|,
  );

  skip "ActiveState perl produces additional warnings", scalar @warnings_regexes
    if ($^O eq 'MSWin32');

  my @warn_output;
  {
      local $SIG{__WARN__} = sub { push(@warn_output, @_) };
      DBICTest::Schema::1->connect($make_dbictest_db::dsn);
  }

  like(shift @warn_output, $_) foreach (@warnings_regexes);

  rmtree($dump_path, 1, 1);
}

eval { DBICTest::Schema::2->connect($make_dbictest_db::dsn) };
ok(!$@, 'no death with dump_directory set (overwrite1)')
    or diag "Dump failed: $@";

DBICTest::Schema::2->_loader_invoked(undef);
eval { DBICTest::Schema::2->connect($make_dbictest_db::dsn) };
ok(!$@, 'no death with dump_directory set (overwrite2)')
    or diag "Dump failed: $@";

END { rmtree($dump_path, 1, 1); }
