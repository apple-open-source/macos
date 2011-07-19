use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

plan tests => 4;

my $artist = $schema->resultset ('Artist')->first;
ok (!$artist->get_dirty_columns, 'Artist is clean' );

$artist->rank (13);
ok (!$artist->get_dirty_columns, 'Artist is clean after num value update' );
$artist->discard_changes;

$artist->rank ('13.00');
ok (!$artist->get_dirty_columns, 'Artist is clean after string value update' );
$artist->discard_changes;

# override column info
$artist->result_source->column_info ('rank')->{is_numeric} = 0;
$artist->rank ('13.00');
ok ($artist->get_dirty_columns, 'Artist is updated after is_numeric override' );
$artist->discard_changes;
