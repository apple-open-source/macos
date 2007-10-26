package Order;

BEGIN { unshift @INC, './t/testlib'; }

use strict;
use base 'CDBase';

__PACKAGE__->table('orders');
__PACKAGE__->table_alias('orders');
__PACKAGE__->columns(Primary => 'film');
__PACKAGE__->columns(Others =>   qw/orders/);

sub CONSTRUCT {
	my $class = shift;
	$class->create_orders_table;
}

sub create_orders_table {
	my $class = shift;
	$class->db_Main->do(
		qq{
     CREATE TABLE orders (
        film     VARCHAR(255),
        orders   INTEGER
    )
  	}
	);
}

1;
