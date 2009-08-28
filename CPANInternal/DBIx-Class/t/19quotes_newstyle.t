use strict;
use warnings;

use Test::More;
use IO::File;

BEGIN {
    eval "use DBD::SQLite";
    plan $@
        ? ( skip_all => 'needs DBD::SQLite for testing' )
        : ( tests => 6 );
}

use lib qw(t/lib);

use_ok('DBICTest');
my $schema = DBICTest->init_schema();

my $orig_debugcb = $schema->storage->debugcb;
my $orig_debug = $schema->storage->debug;

diag('Testing against ' . join(' ', map { $schema->storage->dbh->get_info($_) } qw/17 18/));

my $dsn = $schema->storage->connect_info->[0];
$schema->connection(
  $dsn,
  undef,
  undef,
  { AutoCommit => 1 },
  { quote_char => '`', name_sep => '.' },
);

my $sql = '';
$schema->storage->debugcb(sub { $sql = $_[1] });
$schema->storage->debug(1);

my $rs;

$rs = $schema->resultset('CD')->search(
           { 'me.year' => 2001, 'artist.name' => 'Caterwauler McCrae' },
           { join => 'artist' });
eval { $rs->count };
like($sql, qr/\QSELECT COUNT( * ) FROM `cd` `me`  JOIN `artist` `artist` ON ( `artist`.`artistid` = `me`.`artist` ) WHERE ( `artist`.`name` = ? AND `me`.`year` = ? )\E/, 'got correct SQL for count query with quoting');

my $order = 'year DESC';
$rs = $schema->resultset('CD')->search({},
            { 'order_by' => $order });
eval { $rs->first };
like($sql, qr/ORDER BY `\Q${order}\E`/, 'quoted ORDER BY with DESC (should use a scalarref anyway)');

$rs = $schema->resultset('CD')->search({},
            { 'order_by' => \$order });
eval { $rs->first };
like($sql, qr/ORDER BY \Q${order}\E/, 'did not quote ORDER BY with scalarref');

$schema->connection(
  $dsn,
  undef,
  undef,
  { AutoCommit => 1, quote_char => [qw/[ ]/], name_sep => '.' }
);
$schema->storage->debugcb(sub { $sql = $_[1] });
$schema->storage->debug(1);

$rs = $schema->resultset('CD')->search(
           { 'me.year' => 2001, 'artist.name' => 'Caterwauler McCrae' },
           { join => 'artist' });
eval { $rs->count };
like($sql, qr/\QSELECT COUNT( * ) FROM [cd] [me]  JOIN [artist] [artist] ON ( [artist].[artistid] = [me].[artist] ) WHERE ( [artist].[name] = ? AND [me].[year] = ? )\E/, 'got correct SQL for count query with bracket quoting');

my %data = (
       name => 'Bill',
       order => '12'
);

$schema->connection(
  $dsn,
  undef,
  undef,
  { AutoCommit => 1, quote_char => '`', name_sep => '.' }
);

is($schema->storage->sql_maker->update('group', \%data), 'UPDATE `group` SET `name` = ?, `order` = ?', 'quoted table names for UPDATE');

$schema->storage->debugcb($orig_debugcb);
$schema->storage->debug($orig_debug);
