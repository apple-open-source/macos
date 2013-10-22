package Actor;

BEGIN { unshift @INC, './t/testlib'; }

use strict;
use warnings;

use base 'Class::DBI::Test::SQLite';

__PACKAGE__->set_table('Actor');

__PACKAGE__->columns(Primary   => 'id');
__PACKAGE__->columns(All       => qw/ Name Film Salary /);
__PACKAGE__->columns(TEMP      => qw/ nonpersistent /);
__PACKAGE__->columns(Stringify => 'Name');
__PACKAGE__->add_constructor(salary_between => 'salary >= ? AND salary <= ?');

sub mutator_name_for {
	my ($class, $column) = @_;
	return "set_" . $column->name;
}

sub create_sql {
	return qq{
		id     INTEGER PRIMARY KEY,
		name   CHAR(40),
		film   VARCHAR(255),   
		salary INT
	}
}

1;

