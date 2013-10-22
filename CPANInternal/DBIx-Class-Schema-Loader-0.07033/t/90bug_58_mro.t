use strict;
use warnings;
use Test::More;
use Test::Exception;
use DBIx::Class::Schema::Loader;

# use this if you keep a copy of DBD::Sybase linked to FreeTDS somewhere else
BEGIN {
  if (my $lib_dirs = $ENV{DBICTEST_MSSQL_PERL5LIB}) {
    unshift @INC, $_ for split /:/, $lib_dirs;
  }
}

my ($dsn, $user, $pass);

for (qw/MSSQL_ODBC MSSQL_ADO MSSQL/) {
  next unless $ENV{"DBICTEST_${_}_DSN"};

  $dsn  = $ENV{"DBICTEST_${_}_DSN"};
  $user = $ENV{"DBICTEST_${_}_USER"};
  $pass = $ENV{"DBICTEST_${_}_PASS"};

  last;
}

plan skip_all => 'perl 5.8 required for this test'
    if $] >= 5.009005;

plan ($dsn ? (tests => 1) : (skip_all => 'MSSQL required for this test'));

lives_ok {
    DBIx::Class::Schema::Loader::make_schema_at(
        'DBICTest::Schema',
        { naming => 'current' },
        [ $dsn, $user, $pass ],
    );
} 'dynamic MSSQL schema created using make_schema_at';

done_testing;
