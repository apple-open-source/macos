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
DBICTest->init_schema();

my $orig_debugcb = DBICTest->schema->storage->debugcb;
my $orig_debug = DBICTest->schema->storage->debug;

diag('Testing against ' . join(' ', map { DBICTest->schema->storage->dbh->get_info($_) } qw/17 18/));

DBICTest->schema->storage->sql_maker->quote_char('`');
DBICTest->schema->storage->sql_maker->name_sep('.');

my $sql = '';

DBICTest->schema->storage->debugcb(sub { $sql = $_[1] });
DBICTest->schema->storage->debug(1);

my $rs;

$rs = DBICTest::CD->search(
           { 'me.year' => 2001, 'artist.name' => 'Caterwauler McCrae' },
           { join => 'artist' });
eval { $rs->count };
like($sql, qr/\QSELECT COUNT( * ) FROM `cd` `me`  JOIN `artist` `artist` ON ( `artist`.`artistid` = `me`.`artist` ) WHERE ( `artist`.`name` = ? AND `me`.`year` = ? )\E/, 'got correct SQL for count query with quoting');

my $order = 'year DESC';
$rs = DBICTest::CD->search({},
            { 'order_by' => $order });
eval { $rs->first };
like($sql, qr/ORDER BY `\Q${order}\E`/, 'quoted ORDER BY with DESC (should use a scalarref anyway)');

$rs = DBICTest::CD->search({},
            { 'order_by' => \$order });
eval { $rs->first };
like($sql, qr/ORDER BY \Q${order}\E/, 'did not quote ORDER BY with scalarref');

DBICTest->schema->storage->sql_maker->quote_char([qw/[ ]/]);
DBICTest->schema->storage->sql_maker->name_sep('.');

$rs = DBICTest::CD->search(
           { 'me.year' => 2001, 'artist.name' => 'Caterwauler McCrae' },
           { join => 'artist' });
eval { $rs->count };
like($sql, qr/\QSELECT COUNT( * ) FROM [cd] [me]  JOIN [artist] [artist] ON ( [artist].[artistid] = [me].[artist] ) WHERE ( [artist].[name] = ? AND [me].[year] = ? )\E/, 'got correct SQL for count query with bracket quoting');

my %data = (
       name => 'Bill',
       order => '12'
);

DBICTest->schema->storage->sql_maker->quote_char('`');
DBICTest->schema->storage->sql_maker->name_sep('.');

is(DBICTest->schema->storage->sql_maker->update('group', \%data), 'UPDATE `group` SET `name` = ?, `order` = ?', 'quoted table names for UPDATE');

DBICTest->schema->storage->debugcb($orig_debugcb);
DBICTest->schema->storage->debug($orig_debug);
