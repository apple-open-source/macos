package MyBase;

use strict;
use base qw(Class::DBI);

use vars qw/$dbh/;

my @connect = ("dbi:mysql:test", "", "");

$dbh = DBI->connect(@connect) or die DBI->errstr;
my @table;

END { $dbh->do("DROP TABLE $_") foreach @table }

__PACKAGE__->connection(@connect);

sub set_table {
	my $class = shift;
	$class->table($class->create_test_table);
}

sub create_test_table {
	my $self   = shift;
	my $table  = $self->next_available_table;
	my $create = sprintf "CREATE TABLE $table ( %s )", $self->create_sql;
	push @table, $table;
	$dbh->do($create);
	return $table;
}

sub next_available_table {
	my $self   = shift;
	my @tables = sort @{
		$dbh->selectcol_arrayref(
			qq{
    SHOW TABLES
  }
		)
		};
	my $table = $tables[-1] || "aaa";
	return "z$table";
}

1;
