use strict;
use warnings;
use Test::More;
use Test::Exception;
use DBIx::Class::Schema::Loader 'make_schema_at';
use DBIx::Class::Schema::Loader::Utils qw/slurp_file split_name/;
use Try::Tiny;
use File::Path 'rmtree';
use String::ToIdentifier::EN::Unicode 'to_identifier';
use namespace::clean;

use lib qw(t/lib);
use dbixcsl_common_tests ();
use dbixcsl_test_dir '$tdir';

use constant EXTRA_DUMP_DIR => "$tdir/ora_extra_dump";

my $dsn      = $ENV{DBICTEST_ORA_DSN} || '';
my $user     = $ENV{DBICTEST_ORA_USER} || '';
my $password = $ENV{DBICTEST_ORA_PASS} || '';

my ($schema, $extra_schema); # for cleanup in END for extra tests

my $auto_inc_cb = sub {
    my ($table, $col) = @_;
    return (
        qq{ CREATE SEQUENCE ${table}_${col}_seq START WITH 1 INCREMENT BY 1},
        qq{
            CREATE OR REPLACE TRIGGER ${table}_${col}_trigger
            BEFORE INSERT ON ${table}
            FOR EACH ROW
            BEGIN
                SELECT ${table}_${col}_seq.nextval INTO :NEW.${col} FROM dual;
            END;
        }
    );
};

my $auto_inc_drop_cb = sub {
    my ($table, $col) = @_;
    return qq{ DROP SEQUENCE ${table}_${col}_seq };
};

my $tester = dbixcsl_common_tests->new(
    vendor      => 'Oracle',
    auto_inc_pk => 'INTEGER NOT NULL PRIMARY KEY',
    auto_inc_cb => $auto_inc_cb,
    auto_inc_drop_cb => $auto_inc_drop_cb,
    preserve_case_mode_is_exclusive => 1,
    quote_char                      => '"',
    default_is_deferrable => 0,
    default_on_delete_clause => 'NO ACTION',
    default_on_update_clause => 'NO ACTION',
    dsn         => $dsn,
    user        => $user,
    password    => $password,
    data_types  => {
        # From:
        # http://download.oracle.com/docs/cd/B19306_01/server.102/b14200/sql_elements001.htm#i54330
        #
        # These tests require at least Oracle 9.2, because of the VARCHAR to
        # VARCHAR2 casting.
        #
        # Character Types
        'char'         => { data_type => 'char',      size => 1  },
        'char(11)'     => { data_type => 'char',      size => 11 },
        'nchar'        => { data_type => 'nchar',     size => 1  },
        'national character'
                       => { data_type => 'nchar',     size => 1  },
        'nchar(11)'    => { data_type => 'nchar',     size => 11 },
        'national character(11)'
                       => { data_type => 'nchar',     size => 11 },
        'varchar(20)'  => { data_type => 'varchar2',  size => 20 },
        'varchar2(20)' => { data_type => 'varchar2',  size => 20 },
        'nvarchar2(20)'=> { data_type => 'nvarchar2', size => 20 },
        'national character varying(20)'
                       => { data_type => 'nvarchar2', size => 20 },

        # Numeric Types
        #
        # integer/decimal/numeric is alised to NUMBER
        #
        'integer'      => { data_type => 'integer', original => { data_type => 'number', size => [38,0] } },
        'int'          => { data_type => 'integer', original => { data_type => 'number', size => [38,0] } },
        'smallint'     => { data_type => 'integer', original => { data_type => 'number', size => [38,0] } },

        # very long DEFAULT throws an ORA-24345
        "number(15) DEFAULT to_number(decode(substrb(userenv('CLIENT_INFO'),1,1),' ',null,substrb(userenv('CLIENT_INFO'),1,10)))" => {
            data_type => 'numeric', size => [15,0], original => { data_type => 'number' },
            default_value => \"to_number(decode(substrb(userenv('CLIENT_INFO'),1,1),' ',null,substrb(userenv('CLIENT_INFO'),1,10)))"
        },

        'decimal'      => { data_type => 'integer', original => { data_type => 'number', size => [38,0] } },
        'dec'          => { data_type => 'integer', original => { data_type => 'number', size => [38,0] } },
        'numeric'      => { data_type => 'integer', original => { data_type => 'number', size => [38,0] } },

        'decimal(3)'   => { data_type => 'numeric', size => [3,0], original => { data_type => 'number' } },
        'dec(3)'       => { data_type => 'numeric', size => [3,0], original => { data_type => 'number' } },
        'numeric(3)'   => { data_type => 'numeric', size => [3,0], original => { data_type => 'number' } },

        'decimal(3,3)' => { data_type => 'numeric', size => [3,3], original => { data_type => 'number' } },
        'dec(3,3)'     => { data_type => 'numeric', size => [3,3], original => { data_type => 'number' } },
        'numeric(3,3)' => { data_type => 'numeric', size => [3,3], original => { data_type => 'number' } },

        'binary_float'  => { data_type => 'real',             original => { data_type => 'binary_float'  } },
        'binary_double' => { data_type => 'double precision', original => { data_type => 'binary_double' } },

        # these are not mentioned in the summary chart, must be aliased
        real            => { data_type => 'real',             original => { data_type => 'float', size => 63  } },
        'float(63)'     => { data_type => 'real',             original => { data_type => 'float', size => 63  } },
        'float(64)'     => { data_type => 'double precision', original => { data_type => 'float', size => 64  } },
        'float(126)'    => { data_type => 'double precision', original => { data_type => 'float', size => 126 } },
        float           => { data_type => 'double precision', original => { data_type => 'float', size => 126 } },

        # Blob Types
        'raw(50)'      => { data_type => 'raw', size => 50 },
        'clob'         => { data_type => 'clob' },
        'nclob'        => { data_type => 'nclob' },
        'blob'         => { data_type => 'blob' },
        'bfile'        => { data_type => 'bfile' },
        'long'         => { data_type => 'long' },
        'long raw'     => { data_type => 'long raw' },

        # Datetime Types
        'date'         => { data_type => 'datetime', original => { data_type => 'date' } },
        'date default sysdate'
                       => { data_type => 'datetime', default_value => \'current_timestamp',
                            original  => { data_type => 'date', default_value => \'sysdate' } },
        'timestamp'    => { data_type => 'timestamp' },
        'timestamp default current_timestamp'
                       => { data_type => 'timestamp', default_value => \'current_timestamp' },
        'timestamp(3)' => { data_type => 'timestamp', size => 3 },
        'timestamp with time zone'
                       => { data_type => 'timestamp with time zone' },
        'timestamp(3) with time zone'
                       => { data_type => 'timestamp with time zone', size => 3 },
        'timestamp with local time zone'
                       => { data_type => 'timestamp with local time zone' },
        'timestamp(3) with local time zone'
                       => { data_type => 'timestamp with local time zone', size => 3 },
        'interval year to month'
                       => { data_type => 'interval year to month' },
        'interval year(3) to month'
                       => { data_type => 'interval year to month', size => 3 },
        'interval day to second'
                       => { data_type => 'interval day to second' },
        'interval day(3) to second'
                       => { data_type => 'interval day to second', size => [3,6] },
        'interval day to second(3)'
                       => { data_type => 'interval day to second', size => [2,3] },
        'interval day(3) to second(3)'
                       => { data_type => 'interval day to second', size => [3,3] },

        # Other Types
        'rowid'        => { data_type => 'rowid' },
        'urowid'       => { data_type => 'urowid' },
        'urowid(3333)' => { data_type => 'urowid', size => 3333 },
    },
    extra => {
        create => [
            q{
                CREATE TABLE oracle_loader_test1 (
                    id NUMBER(11),
                    value VARCHAR2(100)
                )
            },
            q{ COMMENT ON TABLE oracle_loader_test1 IS 'oracle_loader_test1 table comment' },
            q{ COMMENT ON COLUMN oracle_loader_test1.value IS 'oracle_loader_test1.value column comment' },
            # 4 through 8 are used for the multi-schema tests
            q{
                create table oracle_loader_test9 (
                    id int primary key
                )
            },
            q{
                create table oracle_loader_test10 (
                    id int primary key,
                    nine_id int,
                    foreign key (nine_id) references oracle_loader_test9(id)
                        on delete set null deferrable
                )
            },
        ],
        drop  => [qw/oracle_loader_test1 oracle_loader_test9 oracle_loader_test10/],
        count => 7 + 30 * 2,
        run   => sub {
            my ($monikers, $classes);
            ($schema, $monikers, $classes) = @_;

            SKIP: {
                if (my $source = $monikers->{loader_test1s}) {
                    is $schema->source($source)->column_info('id')->{sequence},
                        'loader_test1s_id_seq',
                        'Oracle sequence detection';
                }
                else {
                    skip 'not running common tests', 1;
                }
            }

            my $class = $classes->{oracle_loader_test1};

            my $filename = $schema->loader->get_dump_filename($class);
            my $code = slurp_file $filename;

            like $code, qr/^=head1 NAME\n\n^$class - oracle_loader_test1 table comment\n\n^=cut\n/m,
                'table comment';

            like $code, qr/^=head2 value\n\n(.+:.+\n)+\noracle_loader_test1\.value column comment\n\n/m,
                'column comment and attrs';

            # test on delete/update fk clause introspection
            ok ((my $rel_info = $schema->source('OracleLoaderTest10')->relationship_info('nine')),
                'got rel info');

            is $rel_info->{attrs}{on_delete}, 'SET NULL',
                'ON DELETE clause introspected correctly';

            is $rel_info->{attrs}{on_update}, 'NO ACTION',
                'ON UPDATE clause set to NO ACTION by default';

            is $rel_info->{attrs}{is_deferrable}, 1,
                'DEFERRABLE clause introspected correctly';

            SKIP: {
                skip 'Set the DBICTEST_ORA_EXTRAUSER_DSN, _USER and _PASS environment variables to run the cross-schema relationship tests', 6 * 2
                    unless $ENV{DBICTEST_ORA_EXTRAUSER_DSN};

                $extra_schema = $schema->clone;
                $extra_schema->connection(@ENV{map "DBICTEST_ORA_EXTRAUSER_$_",
                    qw/DSN USER PASS/
                });

                my $dbh1 = $schema->storage->dbh;
                my $dbh2 = $extra_schema->storage->dbh;

                my ($schema1) = $dbh1->selectrow_array('SELECT USER FROM DUAL');
                my ($schema2) = $dbh2->selectrow_array('SELECT USER FROM DUAL');

                $dbh1->do(<<'EOF');
                    CREATE TABLE oracle_loader_test4 (
                        id INT NOT NULL PRIMARY KEY,
                        value VARCHAR(100)
                    )
EOF

                $dbh1->do($_) for $auto_inc_cb->('oracle_loader_test4', 'id');

                $dbh1->do("GRANT ALL ON oracle_loader_test4 TO $schema2");
                $dbh1->do("GRANT ALL ON oracle_loader_test4_id_seq TO $schema2");

                $dbh1->do(<<"EOF");
                    CREATE TABLE oracle_loader_test5 (
                        id INT NOT NULL PRIMARY KEY,
                        value VARCHAR(100),
                        four_id INT REFERENCES ${schema1}.oracle_loader_test4 (id),
                        CONSTRAINT ora_loader5_uniq UNIQUE (four_id)
                    )
EOF
                $dbh1->do($_) for $auto_inc_cb->('oracle_loader_test5', 'id');
                $dbh1->do("GRANT ALL ON oracle_loader_test5 TO $schema2");
                $dbh1->do("GRANT ALL ON oracle_loader_test5_id_seq TO $schema2");

                $dbh2->do(<<"EOF");
                    CREATE TABLE oracle_loader_test5 (
                        pk INT NOT NULL PRIMARY KEY,
                        value VARCHAR(100),
                        four_id INT REFERENCES ${schema1}.oracle_loader_test4 (id),
                        CONSTRAINT ora_loader5_uniq UNIQUE (four_id)
                    )
EOF
                $dbh2->do($_) for $auto_inc_cb->('oracle_loader_test5', 'pk');
                $dbh2->do("GRANT ALL ON oracle_loader_test5 TO $schema1");
                $dbh2->do("GRANT ALL ON oracle_loader_test5_pk_seq TO $schema1");

                $dbh2->do(<<"EOF");
                    CREATE TABLE oracle_loader_test6 (
                        id INT NOT NULL PRIMARY KEY,
                        value VARCHAR(100),
                        oracle_loader_test4_id INT REFERENCES ${schema1}.oracle_loader_test4 (id)
                    )
EOF
                $dbh2->do($_) for $auto_inc_cb->('oracle_loader_test6', 'id');
                $dbh2->do("GRANT ALL ON oracle_loader_test6 to $schema1");
                $dbh2->do("GRANT ALL ON oracle_loader_test6_id_seq TO $schema1");

                $dbh2->do(<<"EOF");
                    CREATE TABLE oracle_loader_test7 (
                        id INT NOT NULL PRIMARY KEY,
                        value VARCHAR(100),
                        six_id INT UNIQUE REFERENCES ${schema2}.oracle_loader_test6 (id)
                    )
EOF
                $dbh2->do($_) for $auto_inc_cb->('oracle_loader_test7', 'id');
                $dbh2->do("GRANT ALL ON oracle_loader_test7 to $schema1");
                $dbh2->do("GRANT ALL ON oracle_loader_test7_id_seq TO $schema1");

                $dbh1->do(<<"EOF");
                    CREATE TABLE oracle_loader_test8 (
                        id INT NOT NULL PRIMARY KEY,
                        value VARCHAR(100),
                        oracle_loader_test7_id INT REFERENCES ${schema2}.oracle_loader_test7 (id)
                    )
EOF
                $dbh1->do($_) for $auto_inc_cb->('oracle_loader_test8', 'id');
                $dbh1->do("GRANT ALL ON oracle_loader_test8 to $schema2");
                $dbh1->do("GRANT ALL ON oracle_loader_test8_id_seq TO $schema2");

                # We add schema to moniker_parts, so make a monikers hash for
                # the tests, of the form schemanum.tablenum
                my $schema1_moniker = join '', map ucfirst lc, split_name to_identifier $schema1;
                my $schema2_moniker = join '', map ucfirst lc, split_name to_identifier $schema2;

                my %monikers;
                $monikers{'1.5'} = $schema1_moniker . 'OracleLoaderTest5';
                $monikers{'2.5'} = $schema2_moniker . 'OracleLoaderTest5';

                foreach my $db_schema ([$schema1, $schema2], '%') {
                    lives_and {
                        rmtree EXTRA_DUMP_DIR;

                        my @warns;
                        local $SIG{__WARN__} = sub {
                            push @warns, $_[0] unless $_[0] =~ /\bcollides\b/;
                        };

                        make_schema_at(
                            'OracleMultiSchema',
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
                    } qq{dumped schema for "$schema1" and "$schema2" schemas with no warnings};

                    my ($test_schema, $rsrc, $rs, $row, %uniqs, $rel_info);

                    lives_and {
                        ok $test_schema = OracleMultiSchema->connect($dsn, $user, $password);
                    } 'connected test schema';

                    lives_and {
                        ok $rsrc = $test_schema->source('OracleLoaderTest4');
                    } 'got source for table in schema1';

                    is try { $rsrc->column_info('id')->{is_auto_increment} }, 1,
                        'column in schema1';

                    is try { $rsrc->column_info('value')->{data_type} }, 'varchar2',
                        'column in schema1';

                    is try { $rsrc->column_info('value')->{size} }, 100,
                        'column in schema1';

                    lives_and {
                        ok $rs = $test_schema->resultset('OracleLoaderTest4');
                    } 'got resultset for table in schema1';

                    lives_and {
                        ok $row = $rs->create({ value => 'foo' });
                    } 'executed SQL on table in schema1';

                    my $schema1_identifier = join '_', map lc, split_name to_identifier $schema1;

                    $rel_info = try { $rsrc->relationship_info(
                        $schema1_identifier . '_oracle_loader_test5'
                    ) };

                    is_deeply $rel_info->{cond}, {
                        'foreign.four_id' => 'self.id'
                    }, 'relationship in schema1';

                    is $rel_info->{attrs}{accessor}, 'single',
                        'relationship in schema1';

                    is $rel_info->{attrs}{join_type}, 'LEFT',
                        'relationship in schema1';

                    lives_and {
                        ok $rsrc = $test_schema->source($monikers{'1.5'});
                    } 'got source for table in schema1';

                    %uniqs = try { $rsrc->unique_constraints };

                    is keys %uniqs, 2,
                        'got unique and primary constraint in schema1';

                    delete $uniqs{primary};

                    is_deeply ((values %uniqs)[0], ['four_id'],
                        'correct unique constraint in schema1');

                    lives_and {
                        ok $rsrc = $test_schema->source('OracleLoaderTest6');
                    } 'got source for table in schema2';

                    is try { $rsrc->column_info('id')->{is_auto_increment} }, 1,
                        'column in schema2 introspected correctly';

                    is try { $rsrc->column_info('value')->{data_type} }, 'varchar2',
                        'column in schema2 introspected correctly';

                    is try { $rsrc->column_info('value')->{size} }, 100,
                        'column in schema2 introspected correctly';

                    lives_and {
                        ok $rs = $test_schema->resultset('OracleLoaderTest6');
                    } 'got resultset for table in schema2';

                    lives_and {
                        ok $row = $rs->create({ value => 'foo' });
                    } 'executed SQL on table in schema2';

                    $rel_info = try { $rsrc->relationship_info('oracle_loader_test7') };

                    is_deeply $rel_info->{cond}, {
                        'foreign.six_id' => 'self.id'
                    }, 'relationship in schema2';

                    is $rel_info->{attrs}{accessor}, 'single',
                        'relationship in schema2';

                    is $rel_info->{attrs}{join_type}, 'LEFT',
                        'relationship in schema2';

                    lives_and {
                        ok $rsrc = $test_schema->source('OracleLoaderTest7');
                    } 'got source for table in schema2';

                    %uniqs = try { $rsrc->unique_constraints };

                    is keys %uniqs, 2,
                        'got unique and primary constraint in schema2';

                    delete $uniqs{primary};

                    is_deeply ((values %uniqs)[0], ['six_id'],
                        'correct unique constraint in schema2');

                    lives_and {
                        ok $test_schema->source('OracleLoaderTest6')
                            ->has_relationship('oracle_loader_test4');
                    } 'cross-schema relationship in multi-db_schema';

                    lives_and {
                        ok $test_schema->source('OracleLoaderTest4')
                            ->has_relationship('oracle_loader_test6s');
                    } 'cross-schema relationship in multi-db_schema';

                    lives_and {
                        ok $test_schema->source('OracleLoaderTest8')
                            ->has_relationship('oracle_loader_test7');
                    } 'cross-schema relationship in multi-db_schema';

                    lives_and {
                        ok $test_schema->source('OracleLoaderTest7')
                            ->has_relationship('oracle_loader_test8s');
                    } 'cross-schema relationship in multi-db_schema';
                }
            }
        },
    },
);

if( !$dsn || !$user ) {
    $tester->skip_tests('You need to set the DBICTEST_ORA_DSN, _USER, and _PASS environment variables');
}
else {
    $tester->run_tests();
}

END {
    if (not $ENV{SCHEMA_LOADER_TESTS_NOCLEANUP}) {
        if (my $dbh2 = try { $extra_schema->storage->dbh }) {
            my $dbh1 = $schema->storage->dbh;

            try {
                $dbh1->do($_) for $auto_inc_drop_cb->('oracle_loader_test8', 'id');
                $dbh2->do($_) for $auto_inc_drop_cb->('oracle_loader_test7', 'id');
                $dbh2->do($_) for $auto_inc_drop_cb->('oracle_loader_test6', 'id');
                $dbh2->do($_) for $auto_inc_drop_cb->('oracle_loader_test5', 'pk');
                $dbh1->do($_) for $auto_inc_drop_cb->('oracle_loader_test5', 'id');
                $dbh1->do($_) for $auto_inc_drop_cb->('oracle_loader_test4', 'id');
            }
            catch {
                die "Error dropping sequences for cross-schema test tables: $_";
            };

            try {
                $dbh1->do('DROP TABLE oracle_loader_test8');
                $dbh2->do('DROP TABLE oracle_loader_test7');
                $dbh2->do('DROP TABLE oracle_loader_test6');
                $dbh2->do('DROP TABLE oracle_loader_test5');
                $dbh1->do('DROP TABLE oracle_loader_test5');
                $dbh1->do('DROP TABLE oracle_loader_test4');
            }
            catch {
                die "Error dropping cross-schema test tables: $_";
            };
        }

        rmtree EXTRA_DUMP_DIR;
    }
}
# vim:et sw=4 sts=4 tw=0:
