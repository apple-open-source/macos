use strict;
use warnings;
use lib qw(t/lib);
use Test::More;
use Test::Exception;
use DBICTest;
use List::Util 'first';
use Scalar::Util 'reftype';
use File::Spec;
use IO::Handle;

BEGIN {
    eval { require Test::Moose; Test::Moose->import() };
    plan skip_all => "Need Test::Moose to run this test" if $@;
      require DBIx::Class;

    plan skip_all => 'Test needs ' . DBIx::Class::Optional::Dependencies->req_missing_for ('replicated')
      unless DBIx::Class::Optional::Dependencies->req_ok_for ('replicated');
}

use_ok 'DBIx::Class::Storage::DBI::Replicated::Pool';
use_ok 'DBIx::Class::Storage::DBI::Replicated::Balancer';
use_ok 'DBIx::Class::Storage::DBI::Replicated::Replicant';
use_ok 'DBIx::Class::Storage::DBI::Replicated';

use Moose();
use MooseX::Types();
diag "Using Moose version $Moose::VERSION and MooseX::Types version $MooseX::Types::VERSION";

=head1 HOW TO USE

    This is a test of the replicated storage system.  This will work in one of
    two ways, either it was try to fake replication with a couple of SQLite DBs
    and creative use of copy, or if you define a couple of %ENV vars correctly
    will try to test those.  If you do that, it will assume the setup is properly
    replicating.  Your results may vary, but I have demonstrated this to work with
    mysql native replication.

=cut


## ----------------------------------------------------------------------------
## Build a class to hold all our required testing data and methods.
## ----------------------------------------------------------------------------

TESTSCHEMACLASSES: {

    ## --------------------------------------------------------------------- ##
    ## Create an object to contain your replicated stuff.
    ## --------------------------------------------------------------------- ##

    package DBIx::Class::DBI::Replicated::TestReplication;

    use DBICTest;
    use base qw/Class::Accessor::Fast/;

    __PACKAGE__->mk_accessors( qw/schema/ );

    ## Initialize the object

    sub new {
        my ($class, $schema_method) = (shift, shift);
        my $self = $class->SUPER::new(@_);

        $self->schema( $self->init_schema($schema_method) );
        return $self;
    }

    ## Get the Schema and set the replication storage type

    sub init_schema {
        # current SQLT SQLite producer does not handle DROP TABLE IF EXISTS, trap warnings here
        local $SIG{__WARN__} = sub { warn @_ unless $_[0] =~ /no such table.+DROP TABLE/s };

        my ($class, $schema_method) = @_;

        my $method = "get_schema_$schema_method";
        my $schema = $class->$method;

        return $schema;
    }

    sub get_schema_by_storage_type {
      DBICTest->init_schema(
        sqlite_use_file => 1,
        storage_type=>{
          '::DBI::Replicated' => {
            balancer_type=>'::Random',
            balancer_args=>{
              auto_validate_every=>100,
          master_read_weight => 1
            },
          }
        },
        deploy_args=>{
          add_drop_table => 1,
        },
      );
    }

    sub get_schema_by_connect_info {
      DBICTest->init_schema(
        sqlite_use_file => 1,
        storage_type=> '::DBI::Replicated',
        balancer_type=>'::Random',
        balancer_args=> {
          auto_validate_every=>100,
      master_read_weight => 1
        },
        deploy_args=>{
          add_drop_table => 1,
        },
      );
    }

    sub generate_replicant_connect_info {}
    sub replicate {}
    sub cleanup {}

    ## --------------------------------------------------------------------- ##
    ## Add a connect_info option to test option merging.
    ## --------------------------------------------------------------------- ##
    {
    package DBIx::Class::Storage::DBI::Replicated;

    use Moose;

    __PACKAGE__->meta->make_mutable;

    around connect_info => sub {
      my ($next, $self, $info) = @_;
      $info->[3]{master_option} = 1;
      $self->$next($info);
    };

    __PACKAGE__->meta->make_immutable;

    no Moose;
    }

    ## --------------------------------------------------------------------- ##
    ## Subclass for when you are using SQLite for testing, this provides a fake
    ## replication support.
    ## --------------------------------------------------------------------- ##

    package DBIx::Class::DBI::Replicated::TestReplication::SQLite;

    use DBICTest;
    use File::Copy;
    use base 'DBIx::Class::DBI::Replicated::TestReplication';

    __PACKAGE__->mk_accessors(qw/master_path slave_paths/);

    ## Set the master path from DBICTest

    sub new {
        my $class = shift @_;
        my $self = $class->SUPER::new(@_);

        $self->master_path( DBICTest->_sqlite_dbfilename );
        $self->slave_paths([
            File::Spec->catfile(qw/t var DBIxClass_slave1.db/),
            File::Spec->catfile(qw/t var DBIxClass_slave2.db/),
        ]);

        return $self;
    }

    ## Return an Array of ArrayRefs where each ArrayRef is suitable to use for
    ## $storage->connect_info to be used for connecting replicants.

    sub generate_replicant_connect_info {
        my $self = shift @_;
        my @dsn = map {
            "dbi:SQLite:${_}";
        } @{$self->slave_paths};

        my @connect_infos = map { [$_,'','',{AutoCommit=>1}] } @dsn;

        ## Make sure nothing is left over from a failed test
        $self->cleanup;

        ## try a hashref too
        my $c = $connect_infos[0];
        $connect_infos[0] = {
          dsn => $c->[0],
          user => $c->[1],
          password => $c->[2],
          %{ $c->[3] }
        };

        @connect_infos
    }

    ## Do a 'good enough' replication by copying the master dbfile over each of
    ## the slave dbfiles.  If the master is SQLite we do this, otherwise we
    ## just do a one second pause to let the slaves catch up.

    sub replicate {
        my $self = shift @_;
        foreach my $slave (@{$self->slave_paths}) {
            copy($self->master_path, $slave);
        }
    }

    ## Cleanup after ourselves.  Unlink all gthe slave paths.

    sub cleanup {
        my $self = shift @_;
        foreach my $slave (@{$self->slave_paths}) {
            if(-e $slave) {
                unlink $slave;
            }
        }
    }

    ## --------------------------------------------------------------------- ##
    ## Subclass for when you are setting the databases via custom export vars
    ## This is for when you have a replicating database setup that you are
    ## going to test against.  You'll need to define the correct $ENV and have
    ## two slave databases to test against, as well as a replication system
    ## that will replicate in less than 1 second.
    ## --------------------------------------------------------------------- ##

    package DBIx::Class::DBI::Replicated::TestReplication::Custom;
    use base 'DBIx::Class::DBI::Replicated::TestReplication';

    ## Return an Array of ArrayRefs where each ArrayRef is suitable to use for
    ## $storage->connect_info to be used for connecting replicants.

    sub generate_replicant_connect_info {
        return (
            [$ENV{"DBICTEST_SLAVE0_DSN"}, $ENV{"DBICTEST_SLAVE0_DBUSER"}, $ENV{"DBICTEST_SLAVE0_DBPASS"}, {AutoCommit => 1}],
            [$ENV{"DBICTEST_SLAVE1_DSN"}, $ENV{"DBICTEST_SLAVE1_DBUSER"}, $ENV{"DBICTEST_SLAVE1_DBPASS"}, {AutoCommit => 1}],
        );
    }

    ## pause a bit to let the replication catch up

    sub replicate {
        sleep 1;
    }
}

## ----------------------------------------------------------------------------
## Create an object and run some tests
## ----------------------------------------------------------------------------

## Thi first bunch of tests are basic, just make sure all the bits are behaving

my $replicated_class = DBICTest->has_custom_dsn ?
    'DBIx::Class::DBI::Replicated::TestReplication::Custom' :
    'DBIx::Class::DBI::Replicated::TestReplication::SQLite';

my $replicated;

for my $method (qw/by_connect_info by_storage_type/) {
  undef $replicated;
  ok $replicated = $replicated_class->new($method)
      => "Created a replication object $method";

  isa_ok $replicated->schema
      => 'DBIx::Class::Schema';

  isa_ok $replicated->schema->storage
      => 'DBIx::Class::Storage::DBI::Replicated';

  isa_ok $replicated->schema->storage->balancer
      => 'DBIx::Class::Storage::DBI::Replicated::Balancer::Random'
      => 'configured balancer_type';
}

### check that all Storage::DBI methods are handled by ::Replicated
{
  my @storage_dbi_methods = Class::MOP::Class
    ->initialize('DBIx::Class::Storage::DBI')->get_all_method_names;

  my @replicated_methods  = DBIx::Class::Storage::DBI::Replicated->meta
    ->get_all_method_names;

# remove constants and OTHER_CRAP
  @storage_dbi_methods = grep !/^[A-Z_]+\z/, @storage_dbi_methods;

# remove CAG accessors
  @storage_dbi_methods = grep !/_accessor\z/, @storage_dbi_methods;

# remove DBIx::Class (the root parent, with CAG and stuff) methods
  my @root_methods = Class::MOP::Class->initialize('DBIx::Class')
    ->get_all_method_names;
  my %count;
  $count{$_}++ for (@storage_dbi_methods, @root_methods);

  @storage_dbi_methods = grep $count{$_} != 2, @storage_dbi_methods;

# make hashes
  my %storage_dbi_methods;
  @storage_dbi_methods{@storage_dbi_methods} = ();
  my %replicated_methods;
  @replicated_methods{@replicated_methods} = ();

# remove ::Replicated-specific methods
  for my $method (@replicated_methods) {
    delete $replicated_methods{$method}
      unless exists $storage_dbi_methods{$method};
  }
  @replicated_methods = keys %replicated_methods;

# check that what's left is implemented
  %count = ();
  $count{$_}++ for (@storage_dbi_methods, @replicated_methods);

  if ((grep $count{$_} == 2, @storage_dbi_methods) == @storage_dbi_methods) {
    pass 'all DBIx::Class::Storage::DBI methods implemented';
  }
  else {
    my @unimplemented = grep $count{$_} == 1, @storage_dbi_methods;

    fail 'the following DBIx::Class::Storage::DBI methods are unimplemented: '
      . "@unimplemented";
  }
}

ok $replicated->schema->storage->meta
    => 'has a meta object';

isa_ok $replicated->schema->storage->master
    => 'DBIx::Class::Storage::DBI';

isa_ok $replicated->schema->storage->pool
    => 'DBIx::Class::Storage::DBI::Replicated::Pool';

does_ok $replicated->schema->storage->balancer
    => 'DBIx::Class::Storage::DBI::Replicated::Balancer';

ok my @replicant_connects = $replicated->generate_replicant_connect_info
    => 'got replication connect information';

ok my @replicated_storages = $replicated->schema->storage->connect_replicants(@replicant_connects)
    => 'Created some storages suitable for replicants';

our %debug;
$replicated->schema->storage->debug(1);
$replicated->schema->storage->debugcb(sub {
    my ($op, $info) = @_;
    ##warn "\n$op, $info\n";
    %debug = (
        op => $op,
        info => $info,
        dsn => ($info=~m/\[(.+)\]/)[0],
        storage_type => $info=~m/REPLICANT/ ? 'REPLICANT' : 'MASTER',
    );
});

ok my @all_storages = $replicated->schema->storage->all_storages
    => '->all_storages';

is scalar @all_storages,
    3
    => 'correct number of ->all_storages';

is ((grep $_->isa('DBIx::Class::Storage::DBI'), @all_storages),
    3
    => '->all_storages are correct type');

my @all_storage_opts =
  grep { (reftype($_)||'') eq 'HASH' }
    map @{ $_->_connect_info }, @all_storages;

is ((grep $_->{master_option}, @all_storage_opts),
    3
    => 'connect_info was merged from master to replicants');

my @replicant_names = keys %{ $replicated->schema->storage->replicants };

ok @replicant_names, "found replicant names @replicant_names";

## Silence warning about not supporting the is_replicating method if using the
## sqlite dbs.
$replicated->schema->storage->debugobj->silence(1)
  if first { m{^t/} } @replicant_names;

isa_ok $replicated->schema->storage->balancer->current_replicant
    => 'DBIx::Class::Storage::DBI';

$replicated->schema->storage->debugobj->silence(0);

ok $replicated->schema->storage->pool->has_replicants
    => 'does have replicants';

is $replicated->schema->storage->pool->num_replicants => 2
    => 'has two replicants';

does_ok $replicated_storages[0]
    => 'DBIx::Class::Storage::DBI::Replicated::Replicant';

does_ok $replicated_storages[1]
    => 'DBIx::Class::Storage::DBI::Replicated::Replicant';

does_ok $replicated->schema->storage->replicants->{$replicant_names[0]}
    => 'DBIx::Class::Storage::DBI::Replicated::Replicant';

does_ok $replicated->schema->storage->replicants->{$replicant_names[1]}
    => 'DBIx::Class::Storage::DBI::Replicated::Replicant';

## Add some info to the database

$replicated
    ->schema
    ->populate('Artist', [
        [ qw/artistid name/ ],
        [ 4, "Ozric Tentacles"],
    ]);

    is $debug{storage_type}, 'MASTER',
        "got last query from a master: $debug{dsn}";

    like $debug{info}, qr/INSERT/, 'Last was an insert';

## Make sure all the slaves have the table definitions

$replicated->replicate;
$replicated->schema->storage->replicants->{$replicant_names[0]}->active(1);
$replicated->schema->storage->replicants->{$replicant_names[1]}->active(1);

## Silence warning about not supporting the is_replicating method if using the
## sqlite dbs.
$replicated->schema->storage->debugobj->silence(1)
  if first { m{^t/} } @replicant_names;

$replicated->schema->storage->pool->validate_replicants;

$replicated->schema->storage->debugobj->silence(0);

## Make sure we can read the data.

ok my $artist1 = $replicated->schema->resultset('Artist')->find(4)
    => 'Created Result';

## We removed testing here since master read weight is on, so we can't tell in
## advance what storage to expect.  We turn master read weight off a bit lower
## is $debug{storage_type}, 'REPLICANT'
##     => "got last query from a replicant: $debug{dsn}, $debug{info}";

isa_ok $artist1
    => 'DBICTest::Artist';

is $artist1->name, 'Ozric Tentacles'
    => 'Found expected name for first result';

## Check that master_read_weight is honored
{
    no warnings qw/once redefine/;

    local
    *DBIx::Class::Storage::DBI::Replicated::Balancer::Random::_random_number =
    sub { 999 };

    $replicated->schema->storage->balancer->increment_storage;

    is $replicated->schema->storage->balancer->current_replicant,
       $replicated->schema->storage->master
       => 'master_read_weight is honored';

    ## turn it off for the duration of the test
    $replicated->schema->storage->balancer->master_read_weight(0);
    $replicated->schema->storage->balancer->increment_storage;
}

## Add some new rows that only the master will have  This is because
## we overload any type of write operation so that is must hit the master
## database.

$replicated
    ->schema
    ->populate('Artist', [
        [ qw/artistid name/ ],
        [ 5, "Doom's Children"],
        [ 6, "Dead On Arrival"],
        [ 7, "Watergate"],
    ]);

    is $debug{storage_type}, 'MASTER',
        "got last query from a master: $debug{dsn}";

    like $debug{info}, qr/INSERT/, 'Last was an insert';

## Make sure all the slaves have the table definitions
$replicated->replicate;

## Should find some data now

ok my $artist2 = $replicated->schema->resultset('Artist')->find(5)
    => 'Sync succeed';

is $debug{storage_type}, 'REPLICANT'
    => "got last query from a replicant: $debug{dsn}";

isa_ok $artist2
    => 'DBICTest::Artist';

is $artist2->name, "Doom's Children"
    => 'Found expected name for first result';

## What happens when we disconnect all the replicants?

is $replicated->schema->storage->pool->connected_replicants => 2
    => "both replicants are connected";

$replicated->schema->storage->replicants->{$replicant_names[0]}->disconnect;
$replicated->schema->storage->replicants->{$replicant_names[1]}->disconnect;

is $replicated->schema->storage->pool->connected_replicants => 0
    => "both replicants are now disconnected";

## All these should pass, since the database should automatically reconnect

ok my $artist3 = $replicated->schema->resultset('Artist')->find(6)
    => 'Still finding stuff.';

is $debug{storage_type}, 'REPLICANT'
    => "got last query from a replicant: $debug{dsn}";

isa_ok $artist3
    => 'DBICTest::Artist';

is $artist3->name, "Dead On Arrival"
    => 'Found expected name for first result';

is $replicated->schema->storage->pool->connected_replicants => 1
    => "At Least One replicant reconnected to handle the job";

## What happens when we try to select something that doesn't exist?

ok ! $replicated->schema->resultset('Artist')->find(666)
    => 'Correctly failed to find something.';

is $debug{storage_type}, 'REPLICANT'
    => "got last query from a replicant: $debug{dsn}";

## test the reliable option

TESTRELIABLE: {

    $replicated->schema->storage->set_reliable_storage;

    ok $replicated->schema->resultset('Artist')->find(2)
        => 'Read from master 1';

    is $debug{storage_type}, 'MASTER',
        "got last query from a master: $debug{dsn}";

    ok $replicated->schema->resultset('Artist')->find(5)
        => 'Read from master 2';

    is $debug{storage_type}, 'MASTER',
        "got last query from a master: $debug{dsn}";

    $replicated->schema->storage->set_balanced_storage;

    ok $replicated->schema->resultset('Artist')->find(3)
        => 'Read from replicant';

    is $debug{storage_type}, 'REPLICANT',
        "got last query from a replicant: $debug{dsn}";
}

## Make sure when reliable goes out of scope, we are using replicants again

ok $replicated->schema->resultset('Artist')->find(1)
    => 'back to replicant 1.';

    is $debug{storage_type}, 'REPLICANT',
        "got last query from a replicant: $debug{dsn}";

ok $replicated->schema->resultset('Artist')->find(2)
    => 'back to replicant 2.';

    is $debug{storage_type}, 'REPLICANT',
        "got last query from a replicant: $debug{dsn}";

## set all the replicants to inactive, and make sure the balancer falls back to
## the master.

$replicated->schema->storage->replicants->{$replicant_names[0]}->active(0);
$replicated->schema->storage->replicants->{$replicant_names[1]}->active(0);

{
    ## catch the fallback to master warning
    open my $debugfh, '>', \my $fallback_warning;
    my $oldfh = $replicated->schema->storage->debugfh;
    $replicated->schema->storage->debugfh($debugfh);

    ok $replicated->schema->resultset('Artist')->find(2)
        => 'Fallback to master';

    is $debug{storage_type}, 'MASTER',
        "got last query from a master: $debug{dsn}";

    like $fallback_warning, qr/falling back to master/
        => 'emits falling back to master warning';

    $replicated->schema->storage->debugfh($oldfh);
}

$replicated->schema->storage->replicants->{$replicant_names[0]}->active(1);
$replicated->schema->storage->replicants->{$replicant_names[1]}->active(1);

## Silence warning about not supporting the is_replicating method if using the
## sqlite dbs.
$replicated->schema->storage->debugobj->silence(1)
  if first { m{^t/} } @replicant_names;

$replicated->schema->storage->pool->validate_replicants;

$replicated->schema->storage->debugobj->silence(0);

ok $replicated->schema->resultset('Artist')->find(2)
    => 'Returned to replicates';

is $debug{storage_type}, 'REPLICANT',
    "got last query from a replicant: $debug{dsn}";

## Getting slave status tests

SKIP: {
    ## We skip this tests unless you have a custom replicants, since the default
    ## sqlite based replication tests don't support these functions.

    skip 'Cannot Test Replicant Status on Non Replicating Database', 10
     unless DBICTest->has_custom_dsn && $ENV{"DBICTEST_SLAVE0_DSN"};

    $replicated->replicate; ## Give the slaves a chance to catchup.

    ok $replicated->schema->storage->replicants->{$replicant_names[0]}->is_replicating
        => 'Replicants are replicating';

    is $replicated->schema->storage->replicants->{$replicant_names[0]}->lag_behind_master, 0
        => 'Replicant is zero seconds behind master';

    ## Test the validate replicants

    $replicated->schema->storage->pool->validate_replicants;

    is $replicated->schema->storage->pool->active_replicants, 2
        => 'Still have 2 replicants after validation';

    ## Force the replicants to fail the validate test by required their lag to
    ## be negative (ie ahead of the master!)

    $replicated->schema->storage->pool->maximum_lag(-10);
    $replicated->schema->storage->pool->validate_replicants;

    is $replicated->schema->storage->pool->active_replicants, 0
        => 'No way a replicant be be ahead of the master';

    ## Let's be fair to the replicants again.  Let them lag up to 5

    $replicated->schema->storage->pool->maximum_lag(5);
    $replicated->schema->storage->pool->validate_replicants;

    is $replicated->schema->storage->pool->active_replicants, 2
        => 'Both replicants in good standing again';

    ## Check auto validate

    is $replicated->schema->storage->balancer->auto_validate_every, 100
        => "Got the expected value for auto validate";

        ## This will make sure we auto validatge everytime
        $replicated->schema->storage->balancer->auto_validate_every(0);

        ## set all the replicants to inactive, and make sure the balancer falls back to
        ## the master.

        $replicated->schema->storage->replicants->{$replicant_names[0]}->active(0);
        $replicated->schema->storage->replicants->{$replicant_names[1]}->active(0);

        ## Ok, now when we go to run a query, autovalidate SHOULD reconnect

    is $replicated->schema->storage->pool->active_replicants => 0
        => "both replicants turned off";

    ok $replicated->schema->resultset('Artist')->find(5)
        => 'replicant reactivated';

    is $debug{storage_type}, 'REPLICANT',
        "got last query from a replicant: $debug{dsn}";

    is $replicated->schema->storage->pool->active_replicants => 2
        => "both replicants reactivated";
}

## Test the reliably callback

ok my $reliably = sub {

    ok $replicated->schema->resultset('Artist')->find(5)
        => 'replicant reactivated';

    is $debug{storage_type}, 'MASTER',
        "got last query from a master: $debug{dsn}";

} => 'created coderef properly';

$replicated->schema->storage->execute_reliably($reliably);

## Try something with an error

ok my $unreliably = sub {

    ok $replicated->schema->resultset('ArtistXX')->find(5)
        => 'replicant reactivated';

} => 'created coderef properly';

throws_ok {$replicated->schema->storage->execute_reliably($unreliably)}
    qr/Can't find source for ArtistXX/
    => 'Bad coderef throws proper error';

## Make sure replication came back

ok $replicated->schema->resultset('Artist')->find(3)
    => 'replicant reactivated';

is $debug{storage_type}, 'REPLICANT', "got last query from a replicant: $debug{dsn}";

## make sure transactions are set to execute_reliably

ok my $transaction = sub {

    my $id = shift @_;

    $replicated
        ->schema
        ->populate('Artist', [
            [ qw/artistid name/ ],
            [ $id, "Children of the Grave"],
        ]);

    ok my $result = $replicated->schema->resultset('Artist')->find($id)
        => "Found expected artist for $id";

    is $debug{storage_type}, 'MASTER',
        "got last query from a master: $debug{dsn}";

    ok my $more = $replicated->schema->resultset('Artist')->find(1)
        => 'Found expected artist again for 1';

    is $debug{storage_type}, 'MASTER',
        "got last query from a master: $debug{dsn}";

   return ($result, $more);

} => 'Created a coderef properly';

## Test the transaction with multi return
{
    ok my @return = $replicated->schema->txn_do($transaction, 666)
        => 'did transaction';

        is $return[0]->id, 666
            => 'first returned value is correct';

        is $debug{storage_type}, 'MASTER',
            "got last query from a master: $debug{dsn}";

        is $return[1]->id, 1
            => 'second returned value is correct';

        is $debug{storage_type}, 'MASTER',
             "got last query from a master: $debug{dsn}";

}

## Test that asking for single return works
{
    ok my @return = $replicated->schema->txn_do($transaction, 777)
        => 'did transaction';

        is $return[0]->id, 777
            => 'first returned value is correct';

        is $return[1]->id, 1
            => 'second returned value is correct';
}

## Test transaction returning a single value

{
    ok my $result = $replicated->schema->txn_do(sub {
        ok my $more = $replicated->schema->resultset('Artist')->find(1)
        => 'found inside a transaction';
        is $debug{storage_type}, 'MASTER', "got last query from a master: $debug{dsn}";
        return $more;
    }) => 'successfully processed transaction';

    is $result->id, 1
       => 'Got expected single result from transaction';
}

## Make sure replication came back

ok $replicated->schema->resultset('Artist')->find(1)
    => 'replicant reactivated';

is $debug{storage_type}, 'REPLICANT', "got last query from a replicant: $debug{dsn}";

## Test Discard changes

{
    ok my $artist = $replicated->schema->resultset('Artist')->find(2)
        => 'got an artist to test discard changes';

    is $debug{storage_type}, 'REPLICANT', "got last query from a replicant: $debug{dsn}";

    ok $artist->get_from_storage({force_pool=>'master'})
       => 'properly discard changes';

    is $debug{storage_type}, 'MASTER', "got last query from a master: $debug{dsn}";

    ok $artist->discard_changes({force_pool=>'master'})
       => 'properly called discard_changes against master (manual attrs)';

    is $debug{storage_type}, 'MASTER', "got last query from a master: $debug{dsn}";

    ok $artist->discard_changes()
       => 'properly called discard_changes against master (default attrs)';

    is $debug{storage_type}, 'MASTER', "got last query from a master: $debug{dsn}";

    ok $artist->discard_changes({force_pool=>$replicant_names[0]})
       => 'properly able to override the default attributes';

    is $debug{storage_type}, 'REPLICANT', "got last query from a replicant: $debug{dsn}"
}

## Test some edge cases, like trying to do a transaction inside a transaction, etc

{
    ok my $result = $replicated->schema->txn_do(sub {
        return $replicated->schema->txn_do(sub {
            ok my $more = $replicated->schema->resultset('Artist')->find(1)
            => 'found inside a transaction inside a transaction';
            is $debug{storage_type}, 'MASTER', "got last query from a master: $debug{dsn}";
            return $more;
        });
    }) => 'successfully processed transaction';

    is $result->id, 1
       => 'Got expected single result from transaction';
}

{
    ok my $result = $replicated->schema->txn_do(sub {
        return $replicated->schema->storage->execute_reliably(sub {
            return $replicated->schema->txn_do(sub {
                return $replicated->schema->storage->execute_reliably(sub {
                    ok my $more = $replicated->schema->resultset('Artist')->find(1)
                      => 'found inside crazy deep transactions and execute_reliably';
                    is $debug{storage_type}, 'MASTER', "got last query from a master: $debug{dsn}";
                    return $more;
                });
            });
        });
    }) => 'successfully processed transaction';

    is $result->id, 1
       => 'Got expected single result from transaction';
}

## Test the force_pool resultset attribute.

{
    ok my $artist_rs = $replicated->schema->resultset('Artist')
        => 'got artist resultset';

    ## Turn on Forced Pool Storage
    ok my $reliable_artist_rs = $artist_rs->search(undef, {force_pool=>'master'})
        => 'Created a resultset using force_pool storage';

    ok my $artist = $reliable_artist_rs->find(2)
        => 'got an artist result via force_pool storage';

    is $debug{storage_type}, 'MASTER', "got last query from a master: $debug{dsn}";
}

## Test the force_pool resultset attribute part two.

{
    ok my $artist_rs = $replicated->schema->resultset('Artist')
        => 'got artist resultset';

    ## Turn on Forced Pool Storage
    ok my $reliable_artist_rs = $artist_rs->search(undef, {force_pool=>$replicant_names[0]})
        => 'Created a resultset using force_pool storage';

    ok my $artist = $reliable_artist_rs->find(2)
        => 'got an artist result via force_pool storage';

    is $debug{storage_type}, 'REPLICANT', "got last query from a replicant: $debug{dsn}";
}
## Delete the old database files
$replicated->cleanup;

done_testing;

# vim: sw=4 sts=4 :
