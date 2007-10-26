package PgBase;

use strict;
use base 'Class::DBI';

my $db   = $ENV{DBD_PG_DBNAME} || 'template1';
my $user = $ENV{DBD_PG_USER}   || 'postgres';
my $pass = $ENV{DBD_PG_PASSWD} || '';

__PACKAGE__->connection("dbi:Pg:dbname=$db", $user, $pass,
	{ AutoCommit => 1 });

sub CONSTRUCT {
	my $class = shift;
	my ($table, $sequence) = ($class->table, $class->sequence || "");
	my $schema = $class->schema;
	$class->db_Main->do("CREATE TEMPORARY SEQUENCE $sequence") if $sequence;
	$class->db_Main->do("CREATE TEMPORARY TABLE $table ( $schema )");
}

1;

