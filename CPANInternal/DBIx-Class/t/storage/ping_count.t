use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;
use DBIC::SqlMakerTest;

my $ping_count = 0;

{
  local $SIG{__WARN__} = sub {};
  require DBIx::Class::Storage::DBI;

  my $ping = \&DBIx::Class::Storage::DBI::_ping;

  *DBIx::Class::Storage::DBI::_ping = sub {
    $ping_count++;
    goto &$ping;
  };
}


# measure pings around deploy() separately
my $schema = DBICTest->init_schema( sqlite_use_file => 1, no_populate => 1 );

is ($ping_count, 0, 'no _ping() calls during deploy');
$ping_count = 0;



DBICTest->populate_schema ($schema);

# perform some operations and make sure they don't ping

$schema->resultset('CD')->create({
  cdid => 6, artist => 3, title => 'mtfnpy', year => 2009
});

$schema->resultset('CD')->create({
  cdid => 7, artist => 3, title => 'mtfnpy2', year => 2009
});

$schema->storage->_dbh->disconnect;

$schema->resultset('CD')->create({
  cdid => 8, artist => 3, title => 'mtfnpy3', year => 2009
});

$schema->storage->_dbh->disconnect;

$schema->txn_do(sub {
 $schema->resultset('CD')->create({
   cdid => 9, artist => 3, title => 'mtfnpy4', year => 2009
 });
});

is $ping_count, 0, 'no _ping() calls';

done_testing;
