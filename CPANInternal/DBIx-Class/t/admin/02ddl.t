use strict;
use warnings;

use Test::More;
use Test::Exception;
use Test::Warn;

BEGIN {
    require DBIx::Class;
    plan skip_all => 'Test needs ' . DBIx::Class::Optional::Dependencies->req_missing_for('admin')
      unless DBIx::Class::Optional::Dependencies->req_ok_for('admin');

    plan skip_all => 'Test needs ' . DBIx::Class::Optional::Dependencies->req_missing_for('deploy')
      unless DBIx::Class::Optional::Dependencies->req_ok_for('deploy');
}

use lib qw(t/lib);
use DBICTest;

use Path::Class;

use_ok 'DBIx::Class::Admin';


my $sql_dir = dir(qw/t var/);
my @connect_info = DBICTest->_database(
  no_deploy=>1,
  no_populate=>1,
  sqlite_use_file  => 1,
);
{ # create the schema

#  make sure we are  clean
clean_dir($sql_dir);


my $admin = DBIx::Class::Admin->new(
  schema_class=> "DBICTest::Schema",
  sql_dir=> $sql_dir,
  connect_info => \@connect_info, 
);
isa_ok ($admin, 'DBIx::Class::Admin', 'create the admin object');
lives_ok { $admin->create('MySQL'); } 'Can create MySQL sql';
lives_ok { $admin->create('SQLite'); } 'Can Create SQLite sql';
}

{ # upgrade schema

#my $schema = DBICTest->init_schema(
#  no_deploy    => 1,
#  no_populat    => 1,
#  sqlite_use_file  => 1,
#);

clean_dir($sql_dir);
require DBICVersion_v1;

my $admin = DBIx::Class::Admin->new(
  schema_class => 'DBICVersion::Schema', 
  sql_dir =>  $sql_dir,
  connect_info => \@connect_info,
);

my $schema = $admin->schema();

lives_ok { $admin->create($schema->storage->sqlt_type(), {add_drop_table=>0}); } 'Can create DBICVersionOrig sql in ' . $schema->storage->sqlt_type;
lives_ok { $admin->deploy(  ) } 'Can Deploy schema';

# connect to now deployed schema
lives_ok { $schema = DBICVersion::Schema->connect(@{$schema->storage->connect_info()}); } 'Connect to deployed Database';

is($schema->get_db_version, $DBICVersion::Schema::VERSION, 'Schema deployed and versions match');


require DBICVersion_v2;

$admin = DBIx::Class::Admin->new(
  schema_class => 'DBICVersion::Schema', 
  sql_dir =>  $sql_dir,
  connect_info => \@connect_info
);

lives_ok { $admin->create($schema->storage->sqlt_type(), {}, "1.0" ); } 'Can create diff for ' . $schema->storage->sqlt_type;
{
  local $SIG{__WARN__} = sub { warn $_[0] unless $_[0] =~ /DB version .+? is lower than the schema version/ };
  lives_ok {$admin->upgrade();} 'upgrade the schema';
}

is($schema->get_db_version, $DBICVersion::Schema::VERSION, 'Schema and db versions match');

}

{ # install

clean_dir($sql_dir);

my $admin = DBIx::Class::Admin->new(
  schema_class  => 'DBICVersion::Schema', 
  sql_dir      => $sql_dir,
  _confirm    => 1,
  connect_info  => \@connect_info,
);

$admin->version("3.0");
lives_ok { $admin->install(); } 'install schema version 3.0';
is($admin->schema->get_db_version, "3.0", 'db thinks its version 3.0');
dies_ok { $admin->install("4.0"); } 'cannot install to allready existing version';

$admin->force(1);
warnings_exist ( sub {
  lives_ok { $admin->install("4.0") } 'can force install to allready existing version'
}, qr/Forcing install may not be a good idea/, 'Force warning emitted' );
is($admin->schema->get_db_version, "4.0", 'db thinks its version 4.0');
#clean_dir($sql_dir);
}

sub clean_dir {
  my ($dir) = @_;
  $dir = $dir->resolve;
  if ( ! -d $dir ) {
    $dir->mkpath();
  }
  foreach my $file ($dir->children) {
    # skip any hidden files
    next if ($file =~ /^\./); 
    unlink $file;
  }
}

done_testing;
