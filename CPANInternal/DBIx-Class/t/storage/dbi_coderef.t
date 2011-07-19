use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

plan tests => 1;

# Set up the "usual" sqlite for DBICTest
my $normal_schema = DBICTest->init_schema( sqlite_use_file => 1 );

# Steal the dsn, which should be like 'dbi:SQLite:t/var/DBIxClass.db'
my $normal_dsn = $normal_schema->storage->_dbi_connect_info->[0];

# Make sure we have no active connection
$normal_schema->storage->disconnect;

# Make a new clone with a new connection, using a code reference
my $code_ref_schema = $normal_schema->connect(sub { DBI->connect($normal_dsn); });

# Stolen from 60core.t - this just verifies things seem to work at all
my @art = $code_ref_schema->resultset("Artist")->search({ }, { order_by => 'name DESC'});
cmp_ok(@art, '==', 3, "Three artists returned");
