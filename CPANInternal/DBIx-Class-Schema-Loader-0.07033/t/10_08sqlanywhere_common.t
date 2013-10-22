use strict;
use warnings;
use Test::More;
use Test::Exception;
use Try::Tiny;
use File::Path 'rmtree';
use DBIx::Class::Schema::Loader 'make_schema_at';
use Scope::Guard ();

use lib qw(t/lib);

use dbixcsl_common_tests;
use dbixcsl_test_dir '$tdir';

use constant EXTRA_DUMP_DIR => "$tdir/sqlanywhere_extra_dump";

# The default max_cursor_count and max_statement_count settings of 50 are too
# low to run this test.
#
# Setting them to zero is preferred.

my $dbd_sqlanywhere_dsn      = $ENV{DBICTEST_SQLANYWHERE_DSN} || '';
my $dbd_sqlanywhere_user     = $ENV{DBICTEST_SQLANYWHERE_USER} || '';
my $dbd_sqlanywhere_password = $ENV{DBICTEST_SQLANYWHERE_PASS} || '';

my $odbc_dsn      = $ENV{DBICTEST_SQLANYWHERE_ODBC_DSN} || '';
my $odbc_user     = $ENV{DBICTEST_SQLANYWHERE_ODBC_USER} || '';
my $odbc_password = $ENV{DBICTEST_SQLANYWHERE_ODBC_PASS} || '';

my ($schema, $schemas_created); # for cleanup in END for extra tests

my $tester = dbixcsl_common_tests->new(
    vendor      => 'SQLAnywhere',
    auto_inc_pk => 'INTEGER IDENTITY NOT NULL PRIMARY KEY',
    connect_info => [ ($dbd_sqlanywhere_dsn ? {
            dsn         => $dbd_sqlanywhere_dsn,
            user        => $dbd_sqlanywhere_user,
            password    => $dbd_sqlanywhere_password,
        } : ()),
        ($odbc_dsn ? {
            dsn         => $odbc_dsn,
            user        => $odbc_user,
            password    => $odbc_password,
        } : ()),
    ],
    loader_options => { preserve_case => 1 },
    data_types  => {
        # http://infocenter.sybase.com/help/topic/com.sybase.help.sqlanywhere.11.0.1/dbreference_en11/rf-datatypes.html
        #
        # Numeric types
        'bit'         => { data_type => 'bit' },
        'tinyint'     => { data_type => 'tinyint' },
        'smallint'    => { data_type => 'smallint' },
        'int'         => { data_type => 'integer' },
        'integer'     => { data_type => 'integer' },
        'bigint'      => { data_type => 'bigint' },
        'float'       => { data_type => 'real' },
        'real'        => { data_type => 'real' },
        'double'      => { data_type => 'double precision' },
        'double precision' =>
                         { data_type => 'double precision' },

        'float(2)'    => { data_type => 'real' },
        'float(24)'   => { data_type => 'real' },
        'float(25)'   => { data_type => 'double precision' },
        'float(53)'   => { data_type => 'double precision' },

        # This test only works with the default precision and scale options.
        #
        # They are preserved even for the default values, because the defaults
        # can be changed.
        'decimal'     => { data_type => 'decimal', size => [30,6] },
        'dec'         => { data_type => 'decimal', size => [30,6] },
        'numeric'     => { data_type => 'numeric', size => [30,6] },

        'decimal(3)'   => { data_type => 'decimal', size => [3,0] },
        'dec(3)'       => { data_type => 'decimal', size => [3,0] },
        'numeric(3)'   => { data_type => 'numeric', size => [3,0] },

        'decimal(3,3)' => { data_type => 'decimal', size => [3,3] },
        'dec(3,3)'     => { data_type => 'decimal', size => [3,3] },
        'numeric(3,3)' => { data_type => 'numeric', size => [3,3] },

        'decimal(18,18)' => { data_type => 'decimal', size => [18,18] },
        'dec(18,18)'     => { data_type => 'decimal', size => [18,18] },
        'numeric(18,18)' => { data_type => 'numeric', size => [18,18] },

        # money types
        'money'        => { data_type => 'money' },
        'smallmoney'   => { data_type => 'smallmoney' },

        # bit arrays
        'long varbit'  => { data_type => 'long varbit' },
        'long bit varying'
                       => { data_type => 'long varbit' },
        'varbit'       => { data_type => 'varbit', size => 1 },
        'varbit(20)'   => { data_type => 'varbit', size => 20 },
        'bit varying'  => { data_type => 'varbit', size => 1 },
        'bit varying(20)'
                       => { data_type => 'varbit', size => 20 },

        # Date and Time Types
        'date'        => { data_type => 'date' },
        'datetime'    => { data_type => 'datetime' },
        'smalldatetime'
                      => { data_type => 'smalldatetime' },
        'timestamp'   => { data_type => 'timestamp' },
        # rewrite 'current timestamp' as 'current_timestamp'
        'timestamp default current timestamp'
                      => { data_type => 'timestamp', default_value => \'current_timestamp',
                           original => { default_value => \'current timestamp' } },
        'time'        => { data_type => 'time' },

        # String Types
        'char'         => { data_type => 'char',      size => 1  },
        'char(11)'     => { data_type => 'char',      size => 11 },
        'nchar'        => { data_type => 'nchar',     size => 1  },
        'nchar(11)'    => { data_type => 'nchar',     size => 11 },
        'varchar'      => { data_type => 'varchar',   size => 1  },
        'varchar(20)'  => { data_type => 'varchar',   size => 20 },
        'char varying(20)'
                       => { data_type => 'varchar',   size => 20 },
        'character varying(20)'
                       => { data_type => 'varchar',   size => 20 },
        'nvarchar(20)' => { data_type => 'nvarchar',  size => 20 },
        'xml'          => { data_type => 'xml' },
        'uniqueidentifierstr'
                       => { data_type => 'uniqueidentifierstr' },

        # Binary types
        'binary'       => { data_type => 'binary', size => 1 },
        'binary(20)'   => { data_type => 'binary', size => 20 },
        'varbinary'    => { data_type => 'varbinary', size => 1 },
        'varbinary(20)'=> { data_type => 'varbinary', size => 20 },
        'uniqueidentifier'
                       => { data_type => 'uniqueidentifier' },

        # Blob types
        'long binary'  => { data_type => 'long binary' },
        'image'        => { data_type => 'image' },
        'long varchar' => { data_type => 'long varchar' },
        'text'         => { data_type => 'text' },
        'long nvarchar'=> { data_type => 'long nvarchar' },
        'ntext'        => { data_type => 'ntext' },
    },
    extra => {
        count => 30 * 2,
        run => sub {
            SKIP: {
                $schema  = $_[0];
                my $self = $_[3];

                my $connect_info = [@$self{qw/dsn user password/}];

                my $dbh = $schema->storage->dbh;

                try {
                    $dbh->do("CREATE USER dbicsl_test1 identified by 'dbicsl'");
                }
                catch {
                    $schemas_created = 0;
                    skip "no CREATE USER privileges", 30 * 2;
                };

                $dbh->do(<<"EOF");
                    CREATE TABLE dbicsl_test1.sqlanywhere_loader_test4 (
                        id INT IDENTITY NOT NULL PRIMARY KEY,
                        value VARCHAR(100)
                    )
EOF
                $dbh->do(<<"EOF");
                    CREATE TABLE dbicsl_test1.sqlanywhere_loader_test5 (
                        id INT IDENTITY NOT NULL PRIMARY KEY,
                        value VARCHAR(100),
                        four_id INTEGER NOT NULL,
                        CONSTRAINT loader_test5_uniq UNIQUE (four_id),
                        FOREIGN KEY (four_id) REFERENCES dbicsl_test1.sqlanywhere_loader_test4 (id)
                    )
EOF
                $dbh->do("CREATE USER dbicsl_test2 identified by 'dbicsl'");
                $dbh->do(<<"EOF");
                    CREATE TABLE dbicsl_test2.sqlanywhere_loader_test5 (
                        pk INT IDENTITY NOT NULL PRIMARY KEY,
                        value VARCHAR(100),
                        four_id INTEGER NOT NULL,
                        CONSTRAINT loader_test5_uniq UNIQUE (four_id),
                        FOREIGN KEY (four_id) REFERENCES dbicsl_test1.sqlanywhere_loader_test4 (id)
                    )
EOF
                $dbh->do(<<"EOF");
                    CREATE TABLE dbicsl_test2.sqlanywhere_loader_test6 (
                        id INT IDENTITY NOT NULL PRIMARY KEY,
                        value VARCHAR(100),
                        sqlanywhere_loader_test4_id INTEGER,
                        FOREIGN KEY (sqlanywhere_loader_test4_id) REFERENCES dbicsl_test1.sqlanywhere_loader_test4 (id)
                    )
EOF
                $dbh->do(<<"EOF");
                    CREATE TABLE dbicsl_test2.sqlanywhere_loader_test7 (
                        id INT IDENTITY NOT NULL PRIMARY KEY,
                        value VARCHAR(100),
                        six_id INTEGER NOT NULL UNIQUE,
                        FOREIGN KEY (six_id) REFERENCES dbicsl_test2.sqlanywhere_loader_test6 (id)
                    )
EOF
                $dbh->do(<<"EOF");
                    CREATE TABLE dbicsl_test1.sqlanywhere_loader_test8 (
                        id INT IDENTITY NOT NULL PRIMARY KEY,
                        value VARCHAR(100),
                        sqlanywhere_loader_test7_id INTEGER,
                        FOREIGN KEY (sqlanywhere_loader_test7_id) REFERENCES dbicsl_test2.sqlanywhere_loader_test7 (id)
                    )
EOF

                $schemas_created = 1;

                my $guard = Scope::Guard->new(\&extra_cleanup);

                foreach my $db_schema (['dbicsl_test1', 'dbicsl_test2'], '%') {
                    lives_and {
                        rmtree EXTRA_DUMP_DIR;

                        my @warns;
                        local $SIG{__WARN__} = sub {
                            push @warns, $_[0] unless $_[0] =~ /\bcollides\b/;
                        };

                        make_schema_at(
                            'SQLAnywhereMultiSchema',
                            {
                                naming => 'current',
                                db_schema => $db_schema,
                                dump_directory => EXTRA_DUMP_DIR,
                                quiet => 1,
                            },
                            $connect_info,
                        );

                        diag join "\n", @warns if @warns;

                        is @warns, 0;
                    } 'dumped schema for dbicsl_test1 and dbicsl_test2 schemas with no warnings';

                    my ($test_schema, $rsrc, $rs, $row, %uniqs, $rel_info);

                    lives_and {
                        ok $test_schema = SQLAnywhereMultiSchema->connect(@$connect_info);
                    } 'connected test schema';

                    lives_and {
                        ok $rsrc = $test_schema->source('SqlanywhereLoaderTest4');
                    } 'got source for table in schema one';

                    is try { $rsrc->column_info('id')->{is_auto_increment} }, 1,
                        'column in schema one';

                    is try { $rsrc->column_info('value')->{data_type} }, 'varchar',
                        'column in schema one';

                    is try { $rsrc->column_info('value')->{size} }, 100,
                        'column in schema one';

                    lives_and {
                        ok $rs = $test_schema->resultset('SqlanywhereLoaderTest4');
                    } 'got resultset for table in schema one';

                    lives_and {
                        ok $row = $rs->create({ value => 'foo' });
                    } 'executed SQL on table in schema one';

                    $rel_info = try { $rsrc->relationship_info('dbicsl_test1_sqlanywhere_loader_test5') };

                    is_deeply $rel_info->{cond}, {
                        'foreign.four_id' => 'self.id'
                    }, 'relationship in schema one';

                    is $rel_info->{attrs}{accessor}, 'single',
                        'relationship in schema one';

                    is $rel_info->{attrs}{join_type}, 'LEFT',
                        'relationship in schema one';

                    lives_and {
                        ok $rsrc = $test_schema->source('DbicslTest1SqlanywhereLoaderTest5');
                    } 'got source for table in schema one';

                    %uniqs = try { $rsrc->unique_constraints };

                    is keys %uniqs, 2,
                        'got unique and primary constraint in schema one';

                    delete $uniqs{primary};

                    is_deeply ((values %uniqs)[0], ['four_id'],
                        'correct unique constraint in schema one');

                    lives_and {
                        ok $rsrc = $test_schema->source('SqlanywhereLoaderTest6');
                    } 'got source for table in schema two';

                    is try { $rsrc->column_info('id')->{is_auto_increment} }, 1,
                        'column in schema two introspected correctly';

                    is try { $rsrc->column_info('value')->{data_type} }, 'varchar',
                        'column in schema two introspected correctly';

                    is try { $rsrc->column_info('value')->{size} }, 100,
                        'column in schema two introspected correctly';

                    lives_and {
                        ok $rs = $test_schema->resultset('SqlanywhereLoaderTest6');
                    } 'got resultset for table in schema two';

                    lives_and {
                        ok $row = $rs->create({ value => 'foo' });
                    } 'executed SQL on table in schema two';

                    $rel_info = try { $rsrc->relationship_info('sqlanywhere_loader_test7') };

                    is_deeply $rel_info->{cond}, {
                        'foreign.six_id' => 'self.id'
                    }, 'relationship in schema two';

                    is $rel_info->{attrs}{accessor}, 'single',
                        'relationship in schema two';

                    is $rel_info->{attrs}{join_type}, 'LEFT',
                        'relationship in schema two';

                    lives_and {
                        ok $rsrc = $test_schema->source('SqlanywhereLoaderTest7');
                    } 'got source for table in schema two';

                    %uniqs = try { $rsrc->unique_constraints };

                    is keys %uniqs, 2,
                        'got unique and primary constraint in schema two';

                    delete $uniqs{primary};

                    is_deeply ((values %uniqs)[0], ['six_id'],
                        'correct unique constraint in schema two');

                    lives_and {
                        ok $test_schema->source('SqlanywhereLoaderTest6')
                            ->has_relationship('sqlanywhere_loader_test4');
                    } 'cross-schema relationship in multi-db_schema';

                    lives_and {
                        ok $test_schema->source('SqlanywhereLoaderTest4')
                            ->has_relationship('sqlanywhere_loader_test6s');
                    } 'cross-schema relationship in multi-db_schema';

                    lives_and {
                        ok $test_schema->source('SqlanywhereLoaderTest8')
                            ->has_relationship('sqlanywhere_loader_test7');
                    } 'cross-schema relationship in multi-db_schema';

                    lives_and {
                        ok $test_schema->source('SqlanywhereLoaderTest7')
                            ->has_relationship('sqlanywhere_loader_test8s');
                    } 'cross-schema relationship in multi-db_schema';
                }
            }
        },
    },
);

if (not ($dbd_sqlanywhere_dsn || $odbc_dsn)) {
    $tester->skip_tests('You need to set the DBICTEST_SQLANYWHERE_DSN, _USER and _PASS and/or the DBICTEST_SQLANYWHERE_ODBC_DSN, _USER and _PASS environment variables');
}
else {
    $tester->run_tests();
}

sub extra_cleanup {
    if (not $ENV{SCHEMA_LOADER_TESTS_NOCLEANUP}) {
        if ($schemas_created && (my $dbh = try { $schema->storage->dbh })) {
            foreach my $table ('dbicsl_test1.sqlanywhere_loader_test8',
                               'dbicsl_test2.sqlanywhere_loader_test7',
                               'dbicsl_test2.sqlanywhere_loader_test6',
                               'dbicsl_test2.sqlanywhere_loader_test5',
                               'dbicsl_test1.sqlanywhere_loader_test5',
                               'dbicsl_test1.sqlanywhere_loader_test4') {
                try {
                    $dbh->do("DROP TABLE $table");
                }
                catch {
                    diag "Error dropping table: $_";
                };
            }

            foreach my $db_schema (qw/dbicsl_test1 dbicsl_test2/) {
                try {
                    $dbh->do("DROP USER $db_schema");
                }
                catch {
                    diag "Error dropping test user $db_schema: $_";
                };
            }
        }
        rmtree EXTRA_DUMP_DIR;
    }
}
# vim:et sts=4 sw=4 tw=0:
