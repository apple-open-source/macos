package Binary;

BEGIN { unshift @INC, './t/testlib'; }

use strict;
use base 'PgBase';

__PACKAGE__->table(cdbibintest => 'cdbibintest');
__PACKAGE__->sequence('binseq');
__PACKAGE__->columns(All => qw(id bin));

# __PACKAGE__->data_type(bin => DBI::SQL_BINARY);

sub schema { "id INTEGER, bin BYTEA" }

1;

