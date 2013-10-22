package My::DBI;

$|++;
use strict;
use base 'Ima::DBI';
use Test::More tests => 27;

sub new { return bless {}; }

# Test set_db
__PACKAGE__->set_db('test1', 'dbi:ExampleP:', '', '',
	{ AutoCommit => 1, Taint => 0 });
__PACKAGE__->set_db('test2', 'dbi:ExampleP:', '', '',
	{ AutoCommit => 1, foo => 1 });

ok(__PACKAGE__->can('db_test1'), 'set_db("test1")');
ok(__PACKAGE__->can('db_test2'), 'set_db("test2")');

ok eq_array([ sort __PACKAGE__->db_names ], [ sort qw/test1 test2/ ]),
	'db_names';
ok eq_array([ sort __PACKAGE__->db_handles ],
	[ sort (__PACKAGE__->db_test1, __PACKAGE__->db_test2) ]),
	'db_handles';

# Test set_sql
__PACKAGE__->set_sql('test1', 'select foo from bar where yar = ?', 'test1');
__PACKAGE__->set_sql('test2', 'select mode,size,name from ?',      'test2');
__PACKAGE__->set_sql('test3', 'select %s from ?',                  'test1');
__PACKAGE__->set_sql('test4', 'select %s from ?',             'test1', 0);
__PACKAGE__->set_sql('test5', 'select mode,size,name from ?', 'test1');

for (1 .. 5) {
	ok __PACKAGE__->can("sql_test$_"), "SQL for test$_ set up";
}

ok eq_array(
	[ sort __PACKAGE__->sql_names ],
	[ sort qw/test1 test2 test3 test4 test5/ ]
	),
	'sql_names';

my $obj = My::DBI->new;

# Test sql_*

use Cwd;
my $dir = cwd();
my ($col0, $col1, $col2);

# Test execute & fetch
{
	my $sth = $obj->sql_test2;
	isa_ok $sth, 'DBIx::ContextualFetch::st';
	ok $sth->{Taint}, "Taint mode on queries in db1";
	ok $sth->execute([$dir], [ \($col0, $col1, $col2) ]), "Execute";
	my @row_a = $sth->fetch;
	ok eq_array(\@row_a, [ ($col0, $col1, $col2) ]), "Values OK";
	$sth->finish;
}

# Test fetch_hash
{
	my $sth = $obj->sql_test2;
	$sth->execute($dir);
	my %row_hash = $sth->fetch_hash;
	is keys %row_hash, 3, "3 values fetched back in hash";
	eval { 1 while (my %row = $sth->fetch_hash); };
	ok(!$@, "fetch_hash() doesn't blow up at the end of its fetching");
}

# Test fetch_row/fetch_val/fetch_col
{
	my $sth = $obj->sql_test2;

	my @row = $sth->select_row($dir);
	is @row, 3, "select_row got 3 values";

	my $val = $sth->select_val($dir);
	is $val, $row[0], "select_val is first entry in row";

	my @col = $sth->select_col($dir);
	is $val, $col[0], "... and first entry in column";
}

# Test dynamic SQL generation.
{
	my $sth = $obj->sql_test3(join ',', qw/mode size name/);

	ok !$sth->{Taint}, "Taint mode off for queries in db2";
	my $new_sth = $obj->sql_test3(join ',', qw/mode size name/);
	is $new_sth, $sth, 'Cached handles';

	# TODO: {
	# local $TODO = "Clear sth cache";
	# $sth->clear_cache;
	# my $another_sth = $obj->sql_test3(join ', ', qw/mode size name/);
	# isnt $another_sth, $sth, 'Get a new sth after clearing cache';
	# }

	$new_sth = $obj->sql_test3(join ', ', qw/mode name/);
	isnt $new_sth, $sth, 'redefined statement';

	$sth = $obj->sql_test4(join ',', qw/mode size name/);
	isa_ok $sth, 'DBIx::ContextualFetch::st';

	$new_sth = $obj->sql_test4(join ',', qw/mode size name/);
	isa_ok $sth, 'DBIx::ContextualFetch::st';
	isnt $new_sth, $sth, 'cached handles off';
}

{
	my $dbh     = __PACKAGE__->db_test1;
	my $sth5    = __PACKAGE__->sql_test5;
	my $new_dbh = __PACKAGE__->db_test1;
	is $dbh, $new_dbh, 'dbh handle caching';

	# TODO: {
	# local $TODO = "Clear dbh cache";
	# $dbh->clear_cache;
	# my $another_dbh = __PACKAGE__->db_test1;
	# isnt $another_dbh, $dbh, '$dbh->clear_cache';
	#
	# my $new_sth5 = __PACKAGE__->sql_test5;
	# isnt $sth5, $new_sth5, '  handles flushed, too';
	# }
}

eval { Ima::DBI->i_dont_exist; };

# There's some odd precedence problem trying to pass this all at once.
my $ok = $@ =~ /^Can\'t locate object method "i_dont_exist" via package/;
ok $ok, 'Accidental AutoLoader inheritance blocked';
