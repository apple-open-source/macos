use strict;
use warnings;
use Test::More;
use Test::Exception;
use Try::Tiny;
use File::Path 'rmtree';
use DBIx::Class::Schema::Loader::Utils 'slurp_file';
use DBIx::Class::Schema::Loader 'make_schema_at';

use lib qw(t/lib);

use dbixcsl_common_tests;
use dbixcsl_test_dir '$tdir';

use constant EXTRA_DUMP_DIR => "$tdir/mysql_extra_dump";

my $dsn         = $ENV{DBICTEST_MYSQL_DSN} || '';
my $user        = $ENV{DBICTEST_MYSQL_USER} || '';
my $password    = $ENV{DBICTEST_MYSQL_PASS} || '';
my $test_innodb = $ENV{DBICTEST_MYSQL_INNODB} || 0;

my $skip_rels_msg = 'You need to set the environment variable DBICTEST_MYSQL_INNODB=1 to test relationships.';

my $innodb = $test_innodb ? q{Engine=InnoDB} : '';

my ($schema, $databases_created); # for cleanup in END for extra tests

my $tester = dbixcsl_common_tests->new(
    vendor            => 'Mysql',
    auto_inc_pk       => 'INTEGER NOT NULL PRIMARY KEY AUTO_INCREMENT',
    innodb            => $innodb,
    dsn               => $dsn,
    user              => $user,
    password          => $password,
    connect_info_opts => { on_connect_call => 'set_strict_mode' },
    loader_options    => { preserve_case => 1 },
    skip_rels         => $test_innodb ? 0 : $skip_rels_msg,
    quote_char        => '`',
    no_inline_rels    => 1,
    no_implicit_rels  => 1,
    default_on_clause => 'RESTRICT',
    data_types  => {
        # http://dev.mysql.com/doc/refman/5.5/en/data-type-overview.html
        # Numeric Types
        'bit'         => { data_type => 'bit', size => 1 },
        'bit(11)'     => { data_type => 'bit', size => 11 },

        'bool'        => { data_type => 'tinyint' },
        'boolean'     => { data_type => 'tinyint' },
        'tinyint'     => { data_type => 'tinyint' },
        'tinyint unsigned'
                      => { data_type => 'tinyint',   extra => { unsigned => 1 } },
        'smallint'    => { data_type => 'smallint' },
        'smallint unsigned'
                      => { data_type => 'smallint',  extra => { unsigned => 1 } },
        'mediumint'   => { data_type => 'mediumint' },
        'mediumint unsigned'
                      => { data_type => 'mediumint', extra => { unsigned => 1 } },
        'int'         => { data_type => 'integer' },
        'int unsigned'
                      => { data_type => 'integer',   extra => { unsigned => 1 } },
        'integer'     => { data_type => 'integer' },
        'integer unsigned'
                      => { data_type => 'integer',   extra => { unsigned => 1 } },
        'integer not null'
                      => { data_type => 'integer' },
        'bigint'      => { data_type => 'bigint' },
        'bigint unsigned'
                      => { data_type => 'bigint',    extra => { unsigned => 1 } },

        'serial'      => { data_type => 'bigint', is_auto_increment => 1, extra => { unsigned => 1 } },

        'float'       => { data_type => 'float' },
        'float unsigned'
                      => { data_type => 'float',     extra => { unsigned => 1 } },
        'double'      => { data_type => 'double precision' },
        'double unsigned'
                      => { data_type => 'double precision', extra => { unsigned => 1 } },
        'double precision' =>
                         { data_type => 'double precision' },
        'double precision unsigned'
                      => { data_type => 'double precision', extra => { unsigned => 1 } },

        # we skip 'real' because its alias depends on the 'REAL AS FLOAT' setting

        'float(2)'    => { data_type => 'float' },
        'float(24)'   => { data_type => 'float' },
        'float(25)'   => { data_type => 'double precision' },

        'float(3,3)'  => { data_type => 'float', size => [3,3] },
        'double(3,3)' => { data_type => 'double precision', size => [3,3] },
        'double precision(3,3)'
                      => { data_type => 'double precision', size => [3,3] },

        'decimal'     => { data_type => 'decimal' },
        'decimal unsigned'
                      => { data_type => 'decimal', extra => { unsigned => 1 } },
        'dec'         => { data_type => 'decimal' },
        'numeric'     => { data_type => 'decimal' },
        'fixed'       => { data_type => 'decimal' },

        'decimal(3)'   => { data_type => 'decimal', size => [3,0] },

        'decimal(3,3)' => { data_type => 'decimal', size => [3,3] },
        'dec(3,3)'     => { data_type => 'decimal', size => [3,3] },
        'numeric(3,3)' => { data_type => 'decimal', size => [3,3] },
        'fixed(3,3)'   => { data_type => 'decimal', size => [3,3] },

        # Date and Time Types
        'date'        => { data_type => 'date', datetime_undef_if_invalid => 1 },
        'datetime'    => { data_type => 'datetime', datetime_undef_if_invalid => 1 },
        'timestamp default current_timestamp'
                      => { data_type => 'timestamp', default_value => \'current_timestamp', datetime_undef_if_invalid => 1 },
        'time'        => { data_type => 'time' },
        'year'        => { data_type => 'year' },
        'year(4)'     => { data_type => 'year' },
        'year(2)'     => { data_type => 'year', size => 2 },

        # String Types
        'char'         => { data_type => 'char',      size => 1  },
        'char(11)'     => { data_type => 'char',      size => 11 },
        'varchar(20)'  => { data_type => 'varchar',   size => 20 },
        'binary'       => { data_type => 'binary',    size => 1  },
        'binary(11)'   => { data_type => 'binary',    size => 11 },
        'varbinary(20)'=> { data_type => 'varbinary', size => 20 },

        'tinyblob'    => { data_type => 'tinyblob' },
        'tinytext'    => { data_type => 'tinytext' },
        'blob'        => { data_type => 'blob' },

        # text(M) types will map to the appropriate type, length is not stored
        'text'        => { data_type => 'text' },

        'mediumblob'  => { data_type => 'mediumblob' },
        'mediumtext'  => { data_type => 'mediumtext' },
        'longblob'    => { data_type => 'longblob' },
        'longtext'    => { data_type => 'longtext' },

        "enum('foo','bar','baz')"
                      => { data_type => 'enum', extra => { list => [qw/foo bar baz/] } },
        "enum('foo \\'bar\\' baz', 'foo ''bar'' quux')"
                      => { data_type => 'enum', extra => { list => [q{foo 'bar' baz}, q{foo 'bar' quux}] } },
        "set('foo \\'bar\\' baz', 'foo ''bar'' quux')"
                      => { data_type => 'set', extra => { list => [q{foo 'bar' baz}, q{foo 'bar' quux}] } },
        "set('foo','bar','baz')"
                      => { data_type => 'set',  extra => { list => [qw/foo bar baz/] } },

        # RT#68717
        "enum('11,10 (<500)/0 DUN','4,90 (<120)/0 EUR') NOT NULL default '11,10 (<500)/0 DUN'"
                      => { data_type => 'enum', extra => { list => ['11,10 (<500)/0 DUN', '4,90 (<120)/0 EUR'] }, default_value => '11,10 (<500)/0 DUN' },
        "set('11_10 (<500)/0 DUN','4_90 (<120)/0 EUR') NOT NULL default '11_10 (<500)/0 DUN'"
                      => { data_type => 'set', extra => { list => ['11_10 (<500)/0 DUN', '4_90 (<120)/0 EUR'] }, default_value => '11_10 (<500)/0 DUN' },
        "enum('19,90 (<500)/0 EUR','4,90 (<120)/0 EUR','7,90 (<200)/0 CHF','300 (<6000)/0 CZK','4,90 (<100)/0 EUR','39 (<900)/0 DKK','299 (<5000)/0 EEK','9,90 (<250)/0 EUR','3,90 (<100)/0 GBP','3000 (<70000)/0 HUF','4000 (<70000)/0 JPY','13,90 (<200)/0 LVL','99 (<2500)/0 NOK','39 (<1000)/0 PLN','1000 (<20000)/0 RUB','49 (<2500)/0 SEK','29 (<600)/0 USD','19,90 (<600)/0 EUR','0 EUR','0 CHF') NOT NULL default '19,90 (<500)/0 EUR'"
                      => { data_type => 'enum', extra => { list => ['19,90 (<500)/0 EUR','4,90 (<120)/0 EUR','7,90 (<200)/0 CHF','300 (<6000)/0 CZK','4,90 (<100)/0 EUR','39 (<900)/0 DKK','299 (<5000)/0 EEK','9,90 (<250)/0 EUR','3,90 (<100)/0 GBP','3000 (<70000)/0 HUF','4000 (<70000)/0 JPY','13,90 (<200)/0 LVL','99 (<2500)/0 NOK','39 (<1000)/0 PLN','1000 (<20000)/0 RUB','49 (<2500)/0 SEK','29 (<600)/0 USD','19,90 (<600)/0 EUR','0 EUR','0 CHF'] }, default_value => '19,90 (<500)/0 EUR' },
    },
    extra => {
        create => [
            qq{
                CREATE TABLE `mysql_loader-test1` (
                    id INT AUTO_INCREMENT PRIMARY KEY COMMENT 'The\15\12Column',
                    value varchar(100)
                ) $innodb COMMENT 'The\15\12Table'
            },
            q{
                CREATE VIEW mysql_loader_test2 AS SELECT * FROM `mysql_loader-test1`
            },
            # RT#68717
            qq{
                CREATE TABLE `mysql_loader_test3` (
                  `ISO3_code` char(3) NOT NULL default '',
                  `lang_pref` enum('de','en','fr','nl','dk','es','se') NOT NULL,
                  `vat` decimal(4,2) default '16.00',
                  `price_group` enum('EUR_DEFAULT','GBP_GBR','EUR_AUT_BEL_FRA_IRL_NLD','EUR_DNK_SWE','EUR_AUT','EUR_BEL','EUR_FIN','EUR_FRA','EUR_IRL','EUR_NLD','EUR_DNK','EUR_POL','EUR_PRT','EUR_SWE','CHF_CHE','DKK_DNK','SEK_SWE','NOK_NOR','USD_USA','CZK_CZE','PLN_POL','RUB_RUS','HUF_HUN','SKK_SVK','JPY_JPN','LVL_LVA','ROL_ROU','EEK_EST') NOT NULL default 'EUR_DEFAULT',
                  `del_group` enum('19,90 (<500)/0 EUR','4,90 (<120)/0 EUR','7,90 (<200)/0 CHF','300 (<6000)/0 CZK','4,90 (<100)/0 EUR','39 (<900)/0 DKK','299 (<5000)/0 EEK','9,90 (<250)/0 EUR','3,90 (<100)/0 GBP','3000 (<70000)/0 HUF','4000 (<70000)/0 JPY','13,90 (<200)/0 LVL','99 (<2500)/0 NOK','39 (<1000)/0 PLN','1000 (<20000)/0 RUB','49 (<2500)/0 SEK','29 (<600)/0 USD','19,90 (<600)/0 EUR','0 EUR','0 CHF') NOT NULL default '19,90 (<500)/0 EUR',
                  `express_del_group` enum('NO','39 EUR (EXPRESS)','59 EUR (EXPRESS)','79 CHF (EXPRESS)','49 EUR (EXPRESS)','990 CZK (EXPRESS)','19,9 EUR (EXPRESS)','290 DKK (EXPRESS)','990 EEK (EXPRESS)','39 GBP (EXPRESS)','14000 HUF (EXPRESS)','49 LVL (EXPRESS)','590 NOK (EXPRESS)','250 PLN (EXPRESS)','490 SEK (EXPRESS)') NOT NULL default 'NO',
                  `pmethod` varchar(255) NOT NULL default 'VISA,MASTER',
                  `delivery_time` varchar(5) default NULL,
                  `express_delivery_time` varchar(5) default NULL,
                  `eu` int(1) default '0',
                  `cod_costs` varchar(12) default NULL,
                  PRIMARY KEY (`ISO3_code`)
                ) $innodb
            },
            # 4 through 10 are used for the multi-schema tests
            qq{
                create table mysql_loader_test11 (
                    id int auto_increment primary key
                ) $innodb
            },
            qq{
                create table mysql_loader_test12 (
                    id int auto_increment primary key,
                    eleven_id int,
                    foreign key (eleven_id) references mysql_loader_test11(id)
                        on delete restrict on update set null
                ) $innodb
            },
        ],
        pre_drop_ddl => [ 'DROP VIEW mysql_loader_test2', ],
        drop => [ 'mysql_loader-test1', 'mysql_loader_test3', 'mysql_loader_test11', 'mysql_loader_test12' ],
        count => 8 + 30 * 2,
        run => sub {
            my ($monikers, $classes);
            ($schema, $monikers, $classes) = @_;

            is $monikers->{'mysql_loader-test1'}, 'MysqlLoaderTest1',
                'table with dash correctly monikerized';

            my $rsrc = $schema->source('MysqlLoaderTest2');

            is $rsrc->column_info('value')->{data_type}, 'varchar',
                'view introspected successfully';

            $rsrc = $schema->source('MysqlLoaderTest3');

            is_deeply $rsrc->column_info('del_group')->{extra}{list}, ['19,90 (<500)/0 EUR','4,90 (<120)/0 EUR','7,90 (<200)/0 CHF','300 (<6000)/0 CZK','4,90 (<100)/0 EUR','39 (<900)/0 DKK','299 (<5000)/0 EEK','9,90 (<250)/0 EUR','3,90 (<100)/0 GBP','3000 (<70000)/0 HUF','4000 (<70000)/0 JPY','13,90 (<200)/0 LVL','99 (<2500)/0 NOK','39 (<1000)/0 PLN','1000 (<20000)/0 RUB','49 (<2500)/0 SEK','29 (<600)/0 USD','19,90 (<600)/0 EUR','0 EUR','0 CHF'],
                'hairy enum introspected correctly';

            my $class    = $classes->{'mysql_loader-test1'};
            my $filename = $schema->loader->get_dump_filename($class);

            my $code = slurp_file $filename;

            like $code, qr/^=head1 NAME\n\n^$class - The\nTable\n\n^=cut\n/m,
                'table comment';

            like $code, qr/^=head2 id\n\n(.+:.+\n)+\nThe\nColumn\n\n/m,
                'column comment and attrs';

            # test on delete/update fk clause introspection
            ok ((my $rel_info = $schema->source('MysqlLoaderTest12')->relationship_info('eleven')),
                'got rel info');

            is $rel_info->{attrs}{on_delete}, 'RESTRICT',
                'ON DELETE clause introspected correctly';

            is $rel_info->{attrs}{on_update}, 'SET NULL',
                'ON UPDATE clause introspected correctly';

            # multischema tests follow
            SKIP: {
                my $dbh = $schema->storage->dbh;

                try {
                    $dbh->do('CREATE DATABASE `dbicsl-test`');
                }
                catch {
                    diag "CREATE DATABASE returned error: '$_'";
                    skip "no CREATE DATABASE privileges", 30 * 2;
                };

                $dbh->do(<<"EOF");
                    CREATE TABLE `dbicsl-test`.mysql_loader_test4 (
                        id INT AUTO_INCREMENT PRIMARY KEY,
                        value VARCHAR(100)
                    ) $innodb
EOF
                $dbh->do(<<"EOF");
                    CREATE TABLE `dbicsl-test`.mysql_loader_test5 (
                        id INT AUTO_INCREMENT PRIMARY KEY,
                        value VARCHAR(100),
                        four_id INTEGER,
                        CONSTRAINT loader_test5_uniq UNIQUE (four_id),
                        FOREIGN KEY (four_id) REFERENCES `dbicsl-test`.mysql_loader_test4 (id)
                    ) $innodb
EOF

                $dbh->do('CREATE DATABASE `dbicsl.test`');

                # Test that keys are correctly cached by naming the primary and
                # unique keys in this table with the same name as a table in
                # the `dbicsl-test` schema differently.
                $dbh->do(<<"EOF");
                    CREATE TABLE `dbicsl.test`.mysql_loader_test5 (
                        pk INT AUTO_INCREMENT PRIMARY KEY,
                        value VARCHAR(100),
                        four_id INTEGER,
                        CONSTRAINT loader_test5_uniq UNIQUE (four_id),
                        FOREIGN KEY (four_id) REFERENCES `dbicsl-test`.mysql_loader_test4 (id)
                    ) $innodb
EOF

                $dbh->do(<<"EOF");
                    CREATE TABLE `dbicsl.test`.mysql_loader_test6 (
                        id INT AUTO_INCREMENT PRIMARY KEY,
                        value VARCHAR(100),
                        mysql_loader_test4_id INTEGER,
                        FOREIGN KEY (mysql_loader_test4_id) REFERENCES `dbicsl-test`.mysql_loader_test4 (id)
                    ) $innodb
EOF
                $dbh->do(<<"EOF");
                    CREATE TABLE `dbicsl.test`.mysql_loader_test7 (
                        id INT AUTO_INCREMENT PRIMARY KEY,
                        value VARCHAR(100),
                        six_id INTEGER UNIQUE,
                        FOREIGN KEY (six_id) REFERENCES `dbicsl.test`.mysql_loader_test6 (id)
                    ) $innodb
EOF
                $dbh->do(<<"EOF");
                    CREATE TABLE `dbicsl-test`.mysql_loader_test8 (
                        id INT AUTO_INCREMENT PRIMARY KEY,
                        value VARCHAR(100),
                        mysql_loader_test7_id INTEGER,
                        FOREIGN KEY (mysql_loader_test7_id) REFERENCES `dbicsl.test`.mysql_loader_test7 (id)
                    ) $innodb
EOF
                # Test dumping a rel to a table that's not part of the dump.
                $dbh->do('CREATE DATABASE `dbicsl_test_ignored`');
                $dbh->do(<<"EOF");
                    CREATE TABLE `dbicsl_test_ignored`.mysql_loader_test9 (
                        id INT AUTO_INCREMENT PRIMARY KEY,
                        value VARCHAR(100)
                    ) $innodb
EOF
                $dbh->do(<<"EOF");
                    CREATE TABLE `dbicsl-test`.mysql_loader_test10 (
                        id INT AUTO_INCREMENT PRIMARY KEY,
                        value VARCHAR(100),
                        mysql_loader_test9_id INTEGER,
                        FOREIGN KEY (mysql_loader_test9_id) REFERENCES `dbicsl_test_ignored`.mysql_loader_test9 (id)
                    ) $innodb
EOF

                $databases_created = 1;

                SKIP: foreach my $db_schema (['dbicsl-test', 'dbicsl.test'], '%') {
                    if ($db_schema eq '%') {
                        try {
                            $dbh->selectall_arrayref('SHOW DATABASES');
                        }
                        catch {
                            skip 'no SHOW DATABASES privileges', 28;
                        }
                    }

                    lives_and {
                        rmtree EXTRA_DUMP_DIR;

                        my @warns;
                        local $SIG{__WARN__} = sub {
                            push @warns, $_[0] unless $_[0] =~ /\bcollides\b/;
                        };

                        make_schema_at(
                            'MySQLMultiSchema',
                            {
                                naming => 'current',
                                db_schema => $db_schema,
                                dump_directory => EXTRA_DUMP_DIR,
                                quiet => 1,
                            },
                            [ $dsn, $user, $password ],
                        );

                        diag join "\n", @warns if @warns;

                        is @warns, 0;
                    } 'dumped schema for "dbicsl-test" and "dbicsl.test" databases with no warnings';

                    my ($test_schema, $rsrc, $rs, $row, %uniqs, $rel_info);

                    lives_and {
                        ok $test_schema = MySQLMultiSchema->connect($dsn, $user, $password);
                    } 'connected test schema';

                    lives_and {
                        ok $rsrc = $test_schema->source('MysqlLoaderTest4');
                    } 'got source for table in database name with dash';

                    is try { $rsrc->column_info('id')->{is_auto_increment} }, 1,
                        'column in database name with dash';

                    is try { $rsrc->column_info('value')->{data_type} }, 'varchar',
                        'column in database name with dash';

                    is try { $rsrc->column_info('value')->{size} }, 100,
                        'column in database name with dash';

                    lives_and {
                        ok $rs = $test_schema->resultset('MysqlLoaderTest4');
                    } 'got resultset for table in database name with dash';

                    lives_and {
                        ok $row = $rs->create({ value => 'foo' });
                    } 'executed SQL on table in database name with dash';

                    SKIP: {
                        skip 'set the environment variable DBICTEST_MYSQL_INNODB=1 to test relationships', 3 unless $test_innodb;

                        $rel_info = try { $rsrc->relationship_info('dbicsl_dash_test_mysql_loader_test5') };

                        is_deeply $rel_info->{cond}, {
                            'foreign.four_id' => 'self.id'
                        }, 'relationship in database name with dash';

                        is $rel_info->{attrs}{accessor}, 'single',
                            'relationship in database name with dash';

                        is $rel_info->{attrs}{join_type}, 'LEFT',
                            'relationship in database name with dash';
                    }

                    lives_and {
                        ok $rsrc = $test_schema->source('DbicslDashTestMysqlLoaderTest5');
                    } 'got source for table in database name with dash';

                    %uniqs = try { $rsrc->unique_constraints };

                    is keys %uniqs, 2,
                        'got unique and primary constraint in database name with dash';

                    delete $uniqs{primary};

                    is_deeply ((values %uniqs)[0], ['four_id'],
                        'unique constraint is correct in database name with dash');

                    lives_and {
                        ok $rsrc = $test_schema->source('MysqlLoaderTest6');
                    } 'got source for table in database name with dot';

                    is try { $rsrc->column_info('id')->{is_auto_increment} }, 1,
                        'column in database name with dot introspected correctly';

                    is try { $rsrc->column_info('value')->{data_type} }, 'varchar',
                        'column in database name with dot introspected correctly';

                    is try { $rsrc->column_info('value')->{size} }, 100,
                        'column in database name with dot introspected correctly';

                    lives_and {
                        ok $rs = $test_schema->resultset('MysqlLoaderTest6');
                    } 'got resultset for table in database name with dot';

                    lives_and {
                        ok $row = $rs->create({ value => 'foo' });
                    } 'executed SQL on table in database name with dot';

                    SKIP: {
                        skip 'set the environment variable DBICTEST_MYSQL_INNODB=1 to test relationships', 3 unless $test_innodb;

                        $rel_info = try { $rsrc->relationship_info('mysql_loader_test7') };

                        is_deeply $rel_info->{cond}, {
                            'foreign.six_id' => 'self.id'
                        }, 'relationship in database name with dot';

                        is $rel_info->{attrs}{accessor}, 'single',
                            'relationship in database name with dot';

                        is $rel_info->{attrs}{join_type}, 'LEFT',
                            'relationship in database name with dot';
                    }

                    lives_and {
                        ok $rsrc = $test_schema->source('MysqlLoaderTest7');
                    } 'got source for table in database name with dot';

                    %uniqs = try { $rsrc->unique_constraints };

                    is keys %uniqs, 2,
                        'got unique and primary constraint in database name with dot';

                    delete $uniqs{primary};

                    is_deeply ((values %uniqs)[0], ['six_id'],
                        'unique constraint is correct in database name with dot');

                    SKIP: {
                        skip 'set the environment variable DBICTEST_MYSQL_INNODB=1 to test relationships', 4 unless $test_innodb;

                        lives_and {
                            ok $test_schema->source('MysqlLoaderTest6')
                                ->has_relationship('mysql_loader_test4');
                        } 'cross-database relationship in multi-db_schema';

                        lives_and {
                            ok $test_schema->source('MysqlLoaderTest4')
                                ->has_relationship('mysql_loader_test6s');
                        } 'cross-database relationship in multi-db_schema';

                        lives_and {
                            ok $test_schema->source('MysqlLoaderTest8')
                                ->has_relationship('mysql_loader_test7');
                        } 'cross-database relationship in multi-db_schema';

                        lives_and {
                            ok $test_schema->source('MysqlLoaderTest7')
                                ->has_relationship('mysql_loader_test8s');
                        } 'cross-database relationship in multi-db_schema';
                    }
                }
            }
        },
    },
);

if( !$dsn || !$user ) {
    $tester->skip_tests('You need to set the DBICTEST_MYSQL_DSN, DBICTEST_MYSQL_USER, and DBICTEST_MYSQL_PASS environment variables');
}
else {
    diag $skip_rels_msg if not $test_innodb;
    $tester->run_tests();
}

END {
    if (not $ENV{SCHEMA_LOADER_TESTS_NOCLEANUP}) {
        if ($databases_created && (my $dbh = try { $schema->storage->dbh })) {
            foreach my $table ('`dbicsl-test`.mysql_loader_test10',
                               'dbicsl_test_ignored.mysql_loader_test9',
                               '`dbicsl-test`.mysql_loader_test8',
                               '`dbicsl.test`.mysql_loader_test7',
                               '`dbicsl.test`.mysql_loader_test6',
                               '`dbicsl.test`.mysql_loader_test5',
                               '`dbicsl-test`.mysql_loader_test5',
                               '`dbicsl-test`.mysql_loader_test4') {
                try {
                    $dbh->do("DROP TABLE $table");
                }
                catch {
                    diag "Error dropping table: $_";
                };
            }

            foreach my $db (qw/dbicsl-test dbicsl.test dbicsl_test_ignored/) {
                try {
                    $dbh->do("DROP DATABASE `$db`");
                }
                catch {
                    diag "Error dropping test database $db: $_";
                };
            }
        }
        rmtree EXTRA_DUMP_DIR;
    }
}
# vim:et sts=4 sw=4 tw=0:
