package Actor;

BEGIN { unshift @INC, './t/testlib'; }

use base 'CDBase';
use strict;

__PACKAGE__->table('Actor');

__PACKAGE__->columns(Primary => 'id');
__PACKAGE__->columns(All     => qw/ Name Film Salary /);
__PACKAGE__->columns(TEMP    => qw/ nonpersistent /);
__PACKAGE__->add_constructor(salary_between => 'salary >= ? AND salary <= ?');

sub mutator_name { "set_$_[1]" }

sub CONSTRUCT {
	my $class = shift;
	$class->create_actors_table;
}

sub create_actors_table {
	my $class = shift;
	$class->db_Main->do(
		qq{
			CREATE TABLE Actor (
				id     INTEGER PRIMARY KEY,
				name   CHAR(40),
				film   VARCHAR(255),   
				salary INT
     )
	}
	);
}

1;
