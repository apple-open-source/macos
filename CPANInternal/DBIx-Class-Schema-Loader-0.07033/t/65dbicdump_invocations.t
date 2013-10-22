#!perl

use strict;
use warnings;

use Test::More;
use DBIx::Class::Schema::Loader::Utils 'slurp_file';
use File::Path 'rmtree';
use namespace::clean;
use lib 't/lib';
use make_dbictest_db ();
use dbixcsl_test_dir '$tdir';

plan tests => 3;

# Test the -I option

dbicdump(
    '-I', 't/lib', '-o', 'schema_base_class=TestSchemaBaseClass', 'DBICTest::Schema',
    $make_dbictest_db::dsn
);

dbicdump(
    '-It/lib', '-o', 'schema_base_class=TestSchemaBaseClass', 'DBICTest::Schema',
    $make_dbictest_db::dsn
);

dbicdump(
    '-I/dummy', '-It/lib', '-o', 'schema_base_class=TestSchemaBaseClass',
    'DBICTest::Schema',
    $make_dbictest_db::dsn
);

done_testing;

sub dbicdump {
    system $^X, 'script/dbicdump',
        '-o', "dump_directory=$tdir",
        '-o', 'quiet=1',
        @_;

    is $? >> 8, 0,
        'dbicdump executed successfully';
}

END { rmtree $tdir }
