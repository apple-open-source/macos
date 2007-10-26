package # hide from PAUSE 
    Actor;

BEGIN { unshift @INC, './t/testlib'; }

use strict;
use warnings;

use base 'DBIx::Class::Test::SQLite';

__PACKAGE__->set_table('Actor');

__PACKAGE__->columns(Primary => 'id');
__PACKAGE__->columns(All     => qw/ Name Film Salary /);
__PACKAGE__->columns(TEMP    => qw/ nonpersistent /);
__PACKAGE__->add_constructor(salary_between => 'salary >= ? AND salary <= ?');

sub mutator_name { "set_$_[1]" }

sub create_sql {
	return qq{
		id     INTEGER PRIMARY KEY,
		name   CHAR(40),
		film   VARCHAR(255),   
		salary INT
	}
}

1;

