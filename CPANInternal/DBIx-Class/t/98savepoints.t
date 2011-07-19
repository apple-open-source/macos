use strict;
use warnings;

use Test::More;
use lib qw(t/lib);
use DBICTest;
use DBICTest::Stats;

my ($create_sql, $dsn, $user, $pass);

if ($ENV{DBICTEST_PG_DSN}) {
  ($dsn, $user, $pass) = @ENV{map { "DBICTEST_PG_${_}" } qw/DSN USER PASS/};

  $create_sql = "CREATE TABLE artist (artistid serial PRIMARY KEY, name VARCHAR(100), rank INTEGER NOT NULL DEFAULT '13', charfield CHAR(10))";
} elsif ($ENV{DBICTEST_MYSQL_DSN}) {
  ($dsn, $user, $pass) = @ENV{map { "DBICTEST_MYSQL_${_}" } qw/DSN USER PASS/};

  $create_sql = "CREATE TABLE artist (artistid INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY, name VARCHAR(100), rank INTEGER NOT NULL DEFAULT '13', charfield CHAR(10)) ENGINE=InnoDB";
} else {
  plan skip_all => 'Set DBICTEST_(PG|MYSQL)_DSN _USER and _PASS if you want to run savepoint tests';
}

plan tests => 16;

my $schema = DBICTest::Schema->connect ($dsn,$user,$pass,{ auto_savepoint => 1 });

my $stats = DBICTest::Stats->new;

$schema->storage->debugobj($stats);

$schema->storage->debug(1);

{
    local $SIG{__WARN__} = sub {};
    $schema->storage->dbh->do ('DROP TABLE IF EXISTS artist');
    $schema->storage->dbh->do ($create_sql);
}

$schema->resultset('Artist')->create({ name => 'foo' });

$schema->txn_begin;

my $arty = $schema->resultset('Artist')->find(1);

my $name = $arty->name;

# First off, test a generated savepoint name
$schema->svp_begin;

cmp_ok($stats->{'SVP_BEGIN'}, '==', 1, 'Statistics svp_begin tickled');

$arty->update({ name => 'Jheephizzy' });

$arty->discard_changes;

cmp_ok($arty->name, 'eq', 'Jheephizzy', 'Name changed');

# Rollback the generated name
# Active: 0
$schema->svp_rollback;

cmp_ok($stats->{'SVP_ROLLBACK'}, '==', 1, 'Statistics svp_rollback tickled');

$arty->discard_changes;

cmp_ok($arty->name, 'eq', $name, 'Name rolled back');

$arty->update({ name => 'Jheephizzy'});

# Active: 0 1
$schema->svp_begin('testing1');

$arty->update({ name => 'yourmom' });

# Active: 0 1 2
$schema->svp_begin('testing2');

$arty->update({ name => 'gphat' });
$arty->discard_changes;
cmp_ok($arty->name, 'eq', 'gphat', 'name changed');
# Active: 0 1 2
# Rollback doesn't DESTROY the savepoint, it just rolls back to the value
# at it's conception
$schema->svp_rollback('testing2');
$arty->discard_changes;
cmp_ok($arty->name, 'eq', 'yourmom', 'testing2 reverted');

# Active: 0 1 2 3
$schema->svp_begin('testing3');
$arty->update({ name => 'coryg' });
# Active: 0 1 2 3 4
$schema->svp_begin('testing4');
$arty->update({ name => 'watson' });

# Release 3, which implicitly releases 4
# Active: 0 1 2
$schema->svp_release('testing3');
$arty->discard_changes;
cmp_ok($arty->name, 'eq', 'watson', 'release left data');
# This rolls back savepoint 2
# Active: 0 1 2
$schema->svp_rollback;
$arty->discard_changes;
cmp_ok($arty->name, 'eq', 'yourmom', 'rolled back to 2');

# Rollback the original savepoint, taking us back to the beginning, implicitly
# rolling back savepoint 1 and 2
$schema->svp_rollback('savepoint_0');
$arty->discard_changes;
cmp_ok($arty->name, 'eq', 'foo', 'rolled back to start');

$schema->txn_commit;

# And now to see if txn_do will behave correctly

$schema->txn_do (sub {
    $schema->txn_do (sub {
        $arty->name ('Muff');

        $arty->update;
      });

    eval {
      $schema->txn_do (sub {
          $arty->name ('Moff');

          $arty->update;

          $arty->discard_changes;

          is($arty->name,'Moff','Value updated in nested transaction');

          $schema->storage->dbh->do ("GUARANTEED TO PHAIL");
        });
    };

    ok ($@,'Nested transaction failed (good)');

    $arty->discard_changes;

    is($arty->name,'Muff','auto_savepoint rollback worked');

    $arty->name ('Miff');

    $arty->update;
  });

$arty->discard_changes;

is($arty->name,'Miff','auto_savepoint worked');

cmp_ok($stats->{'SVP_BEGIN'},'==',7,'Correct number of savepoints created');

cmp_ok($stats->{'SVP_RELEASE'},'==',3,'Correct number of savepoints released');

cmp_ok($stats->{'SVP_ROLLBACK'},'==',5,'Correct number of savepoint rollbacks');

END { $schema->storage->dbh->do ("DROP TABLE artist") if defined $schema }

