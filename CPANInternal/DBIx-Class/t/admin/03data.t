use strict;
use warnings;

use Test::More;

use Test::Exception;

BEGIN {
    require DBIx::Class;
    plan skip_all => 'Test needs ' . DBIx::Class::Optional::Dependencies->req_missing_for('admin')
      unless DBIx::Class::Optional::Dependencies->req_ok_for('admin');
}

use lib 't/lib';
use DBICTest;

use_ok 'DBIx::Class::Admin';


{ # test data maniplulation functions

  # create a DBICTest so we can steal its connect info
  my $schema = DBICTest->init_schema(
    sqlite_use_file => 1,
  );

  my $admin = DBIx::Class::Admin->new(
    schema_class=> "DBICTest::Schema",
    connect_info => $schema->storage->connect_info(),
    quiet  => 1,
    _confirm=>1,
  );
  isa_ok ($admin, 'DBIx::Class::Admin', 'create the admin object');

  $admin->insert('Employee', { name => 'Matt' });
  my $employees = $schema->resultset('Employee');
  is ($employees->count(), 1, "insert okay" );

  my $employee = $employees->find(1);
  is($employee->name(),  'Matt', "insert valid" );

  $admin->update('Employee', {name => 'Trout'}, {name => 'Matt'});

  $employee = $employees->find(1);
  is($employee->name(),  'Trout', "update Matt to Trout" );

  $admin->insert('Employee', {name =>'Aran'});

  my $expected_data = [ 
    [$employee->result_source->columns() ],
    [1,1,undef,undef,undef,'Trout'],
    [2,2,undef,undef,undef,'Aran']
  ];
  my $data;
  lives_ok { $data = $admin->select('Employee')} 'can retrive data from database';
  is_deeply($data, $expected_data, 'DB matches whats expected');

  $admin->delete('Employee', {name=>'Trout'});
  my $del_rs  = $employees->search({name => 'Trout'});
  is($del_rs->count(), 0, "delete Trout" );
  is ($employees->count(), 1, "left Aran" );
}

done_testing;
