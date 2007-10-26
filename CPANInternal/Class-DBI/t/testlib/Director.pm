package Director;

BEGIN { unshift @INC, './t/testlib'; }

use strict;
use base 'CDBase';

__PACKAGE__->table('Directors');
__PACKAGE__->columns('All' => qw/ Name Birthday IsInsane /);

sub CONSTRUCT {
	my $class = shift;
	$class->create_directors_table;
}

sub create_directors_table {
	my $class = shift;
	$class->db_Main->do(
		qq{
     CREATE TABLE Directors (
        name                    VARCHAR(80),
        birthday                INTEGER,
        isinsane                INTEGER
     )
  }
	);
}

1;
