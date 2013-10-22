package Order;

BEGIN { unshift @INC, './t/testlib'; }

use strict;
use base 'Class::DBI::Test::SQLite';

__PACKAGE__->set_table('orders');
__PACKAGE__->table_alias('orders');
__PACKAGE__->columns(Primary => 'film');
__PACKAGE__->columns(Others  => qw/orders/);

sub create_sql {
	return qq{
		film     VARCHAR(255),
		orders   INTEGER
	};
}

1;

