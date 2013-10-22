#!perl

use strict;
use warnings;

use Test::More;
use File::Path qw/make_path rmtree/;
use DBIx::Class::Schema::Loader::Utils 'slurp_file';
use Try::Tiny;
use namespace::clean;
use DBIx::Class::Schema::Loader::Optional::Dependencies ();
use lib 't/lib';
use make_dbictest_db ();
use dbixcsl_test_dir '$tdir';

BEGIN {
  use DBIx::Class::Schema::Loader::Optional::Dependencies ();
  plan skip_all => 'Tests needs ' . DBIx::Class::Schema::Loader::Optional::Dependencies->req_missing_for('test_dbicdump_config')
    unless (DBIx::Class::Schema::Loader::Optional::Dependencies->req_ok_for('test_dbicdump_config'));
}

plan tests => 2;

my $config_dir  = "$tdir/dbicdump_config";
make_path $config_dir;
my $config_file = "$config_dir/my.conf";

my $dump_path   = "$tdir/dbicdump_config_dump";

open my $fh, '>', $config_file
    or die "Could not write to $config_file: $!";

print $fh <<"EOF";
schema_class DBICTest::Schema

lib t/lib

<connect_info>
    dsn $make_dbictest_db::dsn
</connect_info>

<loader_options>
    dump_directory    $dump_path
    components        InflateColumn::DateTime
    schema_base_class TestSchemaBaseClass
    quiet             1
</loader_options>
EOF

close $fh;

system $^X, 'script/dbicdump', $config_file;

is $? >> 8, 0,
    'dbicdump executed successfully';

my $foo = try { slurp_file "$dump_path/DBICTest/Schema/Result/Foo.pm" } || '';

like $foo, qr/InflateColumn::DateTime/,
    'loader options read correctly from config_file';

done_testing;

END {
    rmtree($config_dir, 1, 1);
    rmtree($dump_path,  1, 1);
}
