package Director;

BEGIN { unshift @INC, './t/testlib'; }

use strict;
use base 'Class::DBI::Test::SQLite';

__PACKAGE__->set_table('Directors');
__PACKAGE__->columns('All' => qw/ Name Birthday IsInsane /);

sub create_sql {
	return qq{
			name                    VARCHAR(80),
			birthday                INTEGER,
			isinsane                INTEGER
	};
}

1;

