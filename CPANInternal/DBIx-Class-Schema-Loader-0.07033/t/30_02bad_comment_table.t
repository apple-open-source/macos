use strict;
use warnings;
use Test::More;
use Test::Exception;
use Test::Warn;
use DBIx::Class::Schema::Loader::Utils 'slurp_file';
use File::Path;
use Try::Tiny;
use lib qw(t/lib);
use make_dbictest_db_bad_comment_tables;
use dbixcsl_test_dir qw/$tdir/;

my $dump_path = "$tdir/dump";

{
    package DBICTest::Schema::1;
    use base qw/ DBIx::Class::Schema::Loader /;
    __PACKAGE__->loader_options(
        dump_directory => $dump_path,
        quiet => 1,
    );
}

try {
    DBICTest::Schema::1->connect($make_dbictest_db_bad_comment_tables::dsn);
};

plan tests => 1;

my $foo = try { slurp_file("$dump_path/DBICTest/Schema/1/Result/Foo.pm") };
my $bar = try { slurp_file("$dump_path/DBICTest/Schema/1/Result/Bar.pm") };

like($foo, qr/Result::Foo\n/, 'No error from invalid comment tables');

END { rmtree($dump_path, 1, 1); }
