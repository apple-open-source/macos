use strict;
use lib qw(t/lib);
use dbixcsl_common_tests;
use Test::More;

my $dsn         = $ENV{DBICTEST_MYSQL_DSN} || '';
my $user        = $ENV{DBICTEST_MYSQL_USER} || '';
my $password    = $ENV{DBICTEST_MYSQL_PASS} || '';
my $test_innodb = $ENV{DBICTEST_MYSQL_INNODB} || 0;

my $skip_rels_msg = 'You need to set the DBICTEST_MYSQL_INNODB environment variable to test relationships';

my $tester = dbixcsl_common_tests->new(
    vendor           => 'Mysql',
    auto_inc_pk      => 'INTEGER NOT NULL PRIMARY KEY AUTO_INCREMENT',
    innodb           => $test_innodb ? q{Engine=InnoDB} : 0,
    dsn              => $dsn,
    user             => $user,
    password         => $password,
    connect_info_opts=> { on_connect_call => 'set_strict_mode' },
    skip_rels        => $test_innodb ? 0 : $skip_rels_msg,
    no_inline_rels   => 1,
    no_implicit_rels => 1,
    extra            => {
        create => [
            qq{
                CREATE TABLE mysql_loader_test1 (
                    id INTEGER UNSIGNED NOT NULL PRIMARY KEY,
                    value ENUM('foo', 'bar', 'baz')
                )
            },
            qq{
                CREATE TABLE mysql_loader_test2 (
                  id INTEGER UNSIGNED NOT NULL PRIMARY KEY,
                  somets TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
                )
            },
        ],
        drop   => [ qw/ mysql_loader_test1 mysql_loader_test2 / ],
        count  => 5,
        run    => sub {
            my ($schema, $monikers, $classes) = @_;
        
            my $rs = $schema->resultset($monikers->{mysql_loader_test1});
            my $column_info = $rs->result_source->column_info('id');
            
            is($column_info->{extra}->{unsigned}, 1, 'Unsigned MySQL columns');

            $column_info = $rs->result_source->column_info('value');

            like($column_info->{data_type}, qr/^enum$/i, 'MySQL ENUM type');
            is_deeply($column_info->{extra}->{list}, [qw/foo bar baz/],
                      'MySQL ENUM values');

            $rs = $schema->resultset($monikers->{mysql_loader_test2});
            $column_info = $rs->result_source->column_info('somets');
            my $default  = $column_info->{default_value};
            ok ((ref($default) eq 'SCALAR'),
                'CURRENT_TIMESTAMP default_value is a scalar ref');
            like $$default, qr/^CURRENT_TIMESTAMP\z/i,
                'CURRENT_TIMESTAMP default eq "CURRENT_TIMESTAMP"';
        },
    }
);

if( !$dsn || !$user ) {
    $tester->skip_tests('You need to set the DBICTEST_MYSQL_DSN, _USER, and _PASS environment variables');
}
else {
    $tester->run_tests();
}
