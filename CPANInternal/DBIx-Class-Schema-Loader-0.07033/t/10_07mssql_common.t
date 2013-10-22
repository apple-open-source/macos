use strict;
use warnings;
use Test::More;
use Test::Exception;
use DBIx::Class::Schema::Loader::Utils 'warnings_exist_silent';
use Try::Tiny;
use File::Path 'rmtree';
use DBIx::Class::Schema::Loader 'make_schema_at';
use namespace::clean;
use Scope::Guard ();

# use this if you keep a copy of DBD::Sybase linked to FreeTDS somewhere else
BEGIN {
  if (my $lib_dirs = $ENV{DBICTEST_MSSQL_PERL5LIB}) {
    unshift @INC, $_ for split /:/, $lib_dirs;
  }
}

use lib qw(t/lib);

use dbixcsl_common_tests ();
use dbixcsl_test_dir '$tdir';

use constant EXTRA_DUMP_DIR => "$tdir/mssql_extra_dump";

# for extra tests cleanup
my $schema;

my ($dsns, $common_version);

for (qw/MSSQL MSSQL_ODBC MSSQL_ADO/) {
  next unless $ENV{"DBICTEST_${_}_DSN"};

  $dsns->{$_}{dsn} = $ENV{"DBICTEST_${_}_DSN"};
  $dsns->{$_}{user} = $ENV{"DBICTEST_${_}_USER"};
  $dsns->{$_}{password} = $ENV{"DBICTEST_${_}_PASS"};

  require DBI;
  my $dbh = DBI->connect (@{$dsns->{$_}}{qw/dsn user password/}, { RaiseError => 1, PrintError => 0} );
  my $srv_ver = eval {
    $dbh->get_info(18)
      ||
    $dbh->selectrow_hashref('master.dbo.xp_msver ProductVersion')->{Character_Value}
  } || 0;

  my ($maj_srv_ver) = $srv_ver =~ /^(\d+)/;

  if (! defined $common_version or $common_version > $maj_srv_ver ) {
    $common_version = $maj_srv_ver;
  }
}

plan skip_all => 'You need to set the DBICTEST_MSSQL_DSN, _USER and _PASS and/or the DBICTEST_MSSQL_ODBC_DSN, _USER and _PASS environment variables'
  unless $dsns;

my $mssql_2008_new_data_types = {
  date     => { data_type => 'date' },
  time     => { data_type => 'time' },
  'time(0)'=> { data_type => 'time', size => 0 },
  'time(1)'=> { data_type => 'time', size => 1 },
  'time(2)'=> { data_type => 'time', size => 2 },
  'time(3)'=> { data_type => 'time', size => 3 },
  'time(4)'=> { data_type => 'time', size => 4 },
  'time(5)'=> { data_type => 'time', size => 5 },
  'time(6)'=> { data_type => 'time', size => 6 },
  'time(7)'=> { data_type => 'time' },
  datetimeoffset => { data_type => 'datetimeoffset' },
  'datetimeoffset(0)' => { data_type => 'datetimeoffset', size => 0 },
  'datetimeoffset(1)' => { data_type => 'datetimeoffset', size => 1 },
  'datetimeoffset(2)' => { data_type => 'datetimeoffset', size => 2 },
  'datetimeoffset(3)' => { data_type => 'datetimeoffset', size => 3 },
  'datetimeoffset(4)' => { data_type => 'datetimeoffset', size => 4 },
  'datetimeoffset(5)' => { data_type => 'datetimeoffset', size => 5 },
  'datetimeoffset(6)' => { data_type => 'datetimeoffset', size => 6 },
  'datetimeoffset(7)' => { data_type => 'datetimeoffset' },
  datetime2      => { data_type => 'datetime2' },
  'datetime2(0)' => { data_type => 'datetime2', size => 0 },
  'datetime2(1)' => { data_type => 'datetime2', size => 1 },
  'datetime2(2)' => { data_type => 'datetime2', size => 2 },
  'datetime2(3)' => { data_type => 'datetime2', size => 3 },
  'datetime2(4)' => { data_type => 'datetime2', size => 4 },
  'datetime2(5)' => { data_type => 'datetime2', size => 5 },
  'datetime2(6)' => { data_type => 'datetime2', size => 6 },
  'datetime2(7)' => { data_type => 'datetime2' },

  hierarchyid      => { data_type => 'hierarchyid' },
};

my $tester = dbixcsl_common_tests->new(
    vendor      => 'mssql',
    auto_inc_pk => 'INTEGER IDENTITY NOT NULL PRIMARY KEY',
    default_function_def => 'DATETIME DEFAULT getdate()',
    connect_info => [values %$dsns],
    preserve_case_mode_is_exclusive => 1,
    quote_char => [ qw/[ ]/ ],
    basic_date_datatype => ($common_version >= 10) ? 'DATE' : 'SMALLDATETIME',
    default_on_clause => 'NO ACTION',
    data_types => {
        # http://msdn.microsoft.com/en-us/library/ms187752.aspx

        # numeric types
        'int identity' => { data_type => 'integer', is_auto_increment => 1 },
        bigint   => { data_type => 'bigint' },
        int      => { data_type => 'integer' },
        integer  => { data_type => 'integer' },
        smallint => { data_type => 'smallint' },
        tinyint  => { data_type => 'tinyint' },
        money       => { data_type => 'money' },
        smallmoney  => { data_type => 'smallmoney' },
        bit         => { data_type => 'bit' },
        real           => { data_type => 'real' },
        'float(14)'    => { data_type => 'real' },
        'float(24)'    => { data_type => 'real' },
        'float(25)'    => { data_type => 'double precision' },
        'float(53)'    => { data_type => 'double precision' },
        float          => { data_type => 'double precision' },
        'double precision'
                       => { data_type => 'double precision' },
        'numeric(6)'   => { data_type => 'numeric', size => [6,0] },
        'numeric(6,3)' => { data_type => 'numeric', size => [6,3] },
        'decimal(6)'   => { data_type => 'decimal', size => [6,0] },
        'decimal(6,3)' => { data_type => 'decimal', size => [6,3] },
        'dec(6,3)'     => { data_type => 'decimal', size => [6,3] },
        numeric        => { data_type => 'numeric' },
        decimal        => { data_type => 'decimal' },
        dec            => { data_type => 'decimal' },

        # datetime types
        datetime => { data_type => 'datetime' },
        # test rewriting getdate() to current_timestamp
        'datetime default getdate()'
                 => { data_type => 'datetime', default_value => \'current_timestamp',
                      original => { default_value => \'getdate()' } },
        smalldatetime  => { data_type => 'smalldatetime' },

        ($common_version >= 10) ? %$mssql_2008_new_data_types : (),

        # string types
        char           => { data_type => 'char', size => 1 },
        'char(2)'      => { data_type => 'char', size => 2 },
        character      => { data_type => 'char', size => 1 },
        'character(2)' => { data_type => 'char', size => 2 },
        'varchar(2)'   => { data_type => 'varchar', size => 2 },

        nchar          => { data_type => 'nchar', size => 1 },
        'nchar(2)'     => { data_type => 'nchar', size => 2 },
        'nvarchar(2)'  => { data_type => 'nvarchar', size => 2 },

        # binary types
        'binary'       => { data_type => 'binary', size => 1 },
        'binary(2)'    => { data_type => 'binary', size => 2 },
        'varbinary(2)' => { data_type => 'varbinary', size => 2 },

        # blob types
        'varchar(max)'   => { data_type => 'text' },
        text             => { data_type => 'text' },

        'nvarchar(max)'  => { data_type => 'ntext' },
        ntext            => { data_type => 'ntext' },

        'varbinary(max)' => { data_type => 'image' },
        image            => { data_type => 'image' },

        # other types
        timestamp        => { data_type => 'timestamp', inflate_datetime => 0 },
        rowversion       => { data_type => 'rowversion' },
        uniqueidentifier => { data_type => 'uniqueidentifier' },
        sql_variant      => { data_type => 'sql_variant' },
        xml              => { data_type => 'xml' },
    },
    extra => {
        create => [
            q{
                CREATE TABLE [mssql_loader_test1.dot] (
                    id INT IDENTITY NOT NULL PRIMARY KEY,
                    dat VARCHAR(8)
                )
            },
            q{
                CREATE TABLE mssql_loader_test3 (
                    id INT IDENTITY NOT NULL PRIMARY KEY
                )
            },
            q{
                CREATE VIEW mssql_loader_test4 AS
                SELECT * FROM mssql_loader_test3
            },
            # test capitalization of cols in unique constraints and rels
            q{ SET QUOTED_IDENTIFIER ON },
            q{ SET ANSI_NULLS ON },
            q{
                CREATE TABLE [MSSQL_Loader_Test5] (
                    [Id] INT IDENTITY NOT NULL PRIMARY KEY,
                    [FooCol] INT NOT NULL,
                    [BarCol] INT NOT NULL,
                    UNIQUE ([FooCol], [BarCol])
                )
            },
            q{
                CREATE TABLE [MSSQL_Loader_Test6] (
                    [Five_Id] INT REFERENCES [MSSQL_Loader_Test5] ([Id])
                )
            },
            # 8 through 12 are used for the multi-schema tests and 13 through 16 are used for multi-db tests
            q{
                create table mssql_loader_test17 (
                    id int identity primary key
                )
            },
            q{
                create table mssql_loader_test18 (
                    id int identity primary key,
                    seventeen_id int,
                    foreign key (seventeen_id) references mssql_loader_test17(id)
                        on delete set default on update set null
                )
            },
        ],
        pre_drop_ddl => [
            'CREATE TABLE mssql_loader_test3 (id INT IDENTITY NOT NULL PRIMARY KEY)',
            'DROP VIEW mssql_loader_test4',
        ],
        drop   => [
            '[mssql_loader_test1.dot]',
            'mssql_loader_test3',
            'MSSQL_Loader_Test6',
            'MSSQL_Loader_Test5',
            'mssql_loader_test17',
            'mssql_loader_test18',
        ],
        count  => 14 + 30 * 2 + 26 * 2, # extra + multi-schema + mutli-db
        run    => sub {
            my ($monikers, $classes, $self);
            ($schema, $monikers, $classes, $self) = @_;

            my $connect_info = [@$self{qw/dsn user password/}];

# Test that the table above (with '.' in name) gets loaded correctly.
            ok((my $rs = eval {
                $schema->resultset('MssqlLoaderTest1Dot') }),
                'got a resultset for table with dot in name');

            ok((my $from = eval { $rs->result_source->from }),
                'got an $rsrc->from for table with dot in name');

            is ref($from), 'SCALAR', '->table with dot in name is a scalar ref';

            is eval { $$from }, "[mssql_loader_test1.dot]",
                '->table with dot in name has correct name';

# Test capitalization of columns and unique constraints
            ok ((my $rsrc = $schema->resultset($monikers->{mssql_loader_test5})->result_source),
                'got result_source');

            if ($schema->loader->preserve_case) {
                is_deeply [ $rsrc->columns ], [qw/Id FooCol BarCol/],
                    'column name case is preserved with case-sensitive collation';

                my %uniqs = $rsrc->unique_constraints;
                delete $uniqs{primary};

                is_deeply ((values %uniqs)[0], [qw/FooCol BarCol/],
                    'column name case is preserved in unique constraint with case-sensitive collation');
            }
            else {
                is_deeply [ $rsrc->columns ], [qw/id foocol barcol/],
                    'column names are lowercased for case-insensitive collation';

                my %uniqs = $rsrc->unique_constraints;
                delete $uniqs{primary};

                is_deeply ((values %uniqs)[0], [qw/foocol barcol/],
                    'columns in unique constraint lowercased for case-insensitive collation');
            }

            lives_and {
                my $five_row = $schema->resultset($monikers->{mssql_loader_test5})->new_result({});

                if ($schema->loader->preserve_case) {
                    $five_row->foo_col(1);
                    $five_row->bar_col(2);
                }
                else {
                    $five_row->foocol(1);
                    $five_row->barcol(2);
                }
                $five_row->insert;

                my $six_row = $five_row->create_related('mssql_loader_test6s', {});

                is $six_row->five->id, 1;
            } 'relationships for mixed-case tables/columns detected';

# Test that a bad view (where underlying table is gone) is ignored.
            my $dbh = $schema->storage->dbh;
            $dbh->do("DROP TABLE mssql_loader_test3");

            warnings_exist_silent { $schema->rescan }
              qr/^Bad table or view 'mssql_loader_test4'/, 'bad view ignored';

            throws_ok {
                $schema->resultset($monikers->{mssql_loader_test4})
            } qr/Can't find source/,
                'no source registered for bad view';

            # test on delete/update fk clause introspection
            ok ((my $rel_info = $schema->source('MssqlLoaderTest18')->relationship_info('seventeen')),
                'got rel info');

            is $rel_info->{attrs}{on_delete}, 'SET DEFAULT',
                'ON DELETE clause introspected correctly';

            is $rel_info->{attrs}{on_update}, 'SET NULL',
                'ON UPDATE clause introspected correctly';

            is $rel_info->{attrs}{is_deferrable}, 1,
                'is_deferrable defaults to 1';

            SKIP: {
                my $dbh = $schema->storage->dbh;

                try {
                    $dbh->do('CREATE SCHEMA [dbicsl-test]');
                }
                catch {
                    skip "no CREATE SCHEMA privileges", 30 * 2;
                };

                $dbh->do(<<"EOF");
                    CREATE TABLE [dbicsl-test].mssql_loader_test8 (
                        id INT IDENTITY PRIMARY KEY,
                        value VARCHAR(100)
                    )
EOF
                $dbh->do(<<"EOF");
                    CREATE TABLE [dbicsl-test].mssql_loader_test9 (
                        id INT IDENTITY PRIMARY KEY,
                        value VARCHAR(100),
                        eight_id INTEGER NOT NULL,
                        CONSTRAINT loader_test9_uniq UNIQUE (eight_id),
                        FOREIGN KEY (eight_id) REFERENCES [dbicsl-test].mssql_loader_test8 (id)
                    )
EOF
                $dbh->do('CREATE SCHEMA [dbicsl.test]');
                $dbh->do(<<"EOF");
                    CREATE TABLE [dbicsl.test].mssql_loader_test9 (
                        pk INT IDENTITY PRIMARY KEY,
                        value VARCHAR(100),
                        eight_id INTEGER NOT NULL,
                        CONSTRAINT loader_test9_uniq UNIQUE (eight_id),
                        FOREIGN KEY (eight_id) REFERENCES [dbicsl-test].mssql_loader_test8 (id)
                    )
EOF
                $dbh->do(<<"EOF");
                    CREATE TABLE [dbicsl.test].mssql_loader_test10 (
                        id INT IDENTITY PRIMARY KEY,
                        value VARCHAR(100),
                        mssql_loader_test8_id INTEGER,
                        FOREIGN KEY (mssql_loader_test8_id) REFERENCES [dbicsl-test].mssql_loader_test8 (id)
                    )
EOF
                $dbh->do(<<"EOF");
                    CREATE TABLE [dbicsl.test].mssql_loader_test11 (
                        id INT IDENTITY PRIMARY KEY,
                        value VARCHAR(100),
                        ten_id INTEGER NOT NULL UNIQUE,
                        FOREIGN KEY (ten_id) REFERENCES [dbicsl.test].mssql_loader_test10 (id)
                    )
EOF
                $dbh->do(<<"EOF");
                    CREATE TABLE [dbicsl-test].mssql_loader_test12 (
                        id INT IDENTITY PRIMARY KEY,
                        value VARCHAR(100),
                        mssql_loader_test11_id INTEGER,
                        FOREIGN KEY (mssql_loader_test11_id) REFERENCES [dbicsl.test].mssql_loader_test11 (id)
                    )
EOF

                my $guard = Scope::Guard->new(\&cleanup_schemas);

                foreach my $db_schema (['dbicsl-test', 'dbicsl.test'], '%') {
                    lives_and {
                        rmtree EXTRA_DUMP_DIR;

                        my @warns;
                        local $SIG{__WARN__} = sub {
                            push @warns, $_[0] unless $_[0] =~ /\bcollides\b/;
                        };

                        make_schema_at(
                            'MSSQLMultiSchema',
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
                    } 'dumped schema for "dbicsl-test" and "dbicsl.test" schemas with no warnings';

                    my ($test_schema, $rsrc, $rs, $row, %uniqs, $rel_info);

                    lives_and {
                        ok $test_schema = MSSQLMultiSchema->connect(@$connect_info);
                    } 'connected test schema';

                    lives_and {
                        ok $rsrc = $test_schema->source('MssqlLoaderTest8');
                    } 'got source for table in schema name with dash';

                    is try { $rsrc->column_info('id')->{is_auto_increment} }, 1,
                        'column in schema name with dash';

                    is try { $rsrc->column_info('value')->{data_type} }, 'varchar',
                        'column in schema name with dash';

                    is try { $rsrc->column_info('value')->{size} }, 100,
                        'column in schema name with dash';

                    lives_and {
                        ok $rs = $test_schema->resultset('MssqlLoaderTest8');
                    } 'got resultset for table in schema name with dash';

                    lives_and {
                        ok $row = $rs->create({ value => 'foo' });
                    } 'executed SQL on table in schema name with dash';

                    $rel_info = try { $rsrc->relationship_info('dbicsl_dash_test_mssql_loader_test9') };

                    is_deeply $rel_info->{cond}, {
                        'foreign.eight_id' => 'self.id'
                    }, 'relationship in schema name with dash';

                    is $rel_info->{attrs}{accessor}, 'single',
                        'relationship in schema name with dash';

                    is $rel_info->{attrs}{join_type}, 'LEFT',
                        'relationship in schema name with dash';

                    lives_and {
                        ok $rsrc = $test_schema->source('DbicslDashTestMssqlLoaderTest9');
                    } 'got source for table in schema name with dash';

                    %uniqs = try { $rsrc->unique_constraints };

                    is keys %uniqs, 2,
                        'got unique and primary constraint in schema name with dash';

                    delete $uniqs{primary};

                    is_deeply ((values %uniqs)[0], ['eight_id'],
                        'correct unique constraint in schema name with dash');

                    lives_and {
                        ok $rsrc = $test_schema->source('MssqlLoaderTest10');
                    } 'got source for table in schema name with dot';

                    is try { $rsrc->column_info('id')->{is_auto_increment} }, 1,
                        'column in schema name with dot introspected correctly';

                    is try { $rsrc->column_info('value')->{data_type} }, 'varchar',
                        'column in schema name with dot introspected correctly';

                    is try { $rsrc->column_info('value')->{size} }, 100,
                        'column in schema name with dot introspected correctly';

                    lives_and {
                        ok $rs = $test_schema->resultset('MssqlLoaderTest10');
                    } 'got resultset for table in schema name with dot';

                    lives_and {
                        ok $row = $rs->create({ value => 'foo' });
                    } 'executed SQL on table in schema name with dot';

                    $rel_info = try { $rsrc->relationship_info('mssql_loader_test11') };

                    is_deeply $rel_info->{cond}, {
                        'foreign.ten_id' => 'self.id'
                    }, 'relationship in schema name with dot';

                    is $rel_info->{attrs}{accessor}, 'single',
                        'relationship in schema name with dot';

                    is $rel_info->{attrs}{join_type}, 'LEFT',
                        'relationship in schema name with dot';

                    lives_and {
                        ok $rsrc = $test_schema->source('MssqlLoaderTest11');
                    } 'got source for table in schema name with dot';

                    %uniqs = try { $rsrc->unique_constraints };

                    is keys %uniqs, 2,
                        'got unique and primary constraint in schema name with dot';

                    delete $uniqs{primary};

                    is_deeply ((values %uniqs)[0], ['ten_id'],
                        'correct unique constraint in schema name with dot');

                    lives_and {
                        ok $test_schema->source('MssqlLoaderTest10')
                            ->has_relationship('mssql_loader_test8');
                    } 'cross-schema relationship in multi-db_schema';

                    lives_and {
                        ok $test_schema->source('MssqlLoaderTest8')
                            ->has_relationship('mssql_loader_test10s');
                    } 'cross-schema relationship in multi-db_schema';

                    lives_and {
                        ok $test_schema->source('MssqlLoaderTest12')
                            ->has_relationship('mssql_loader_test11');
                    } 'cross-schema relationship in multi-db_schema';

                    lives_and {
                        ok $test_schema->source('MssqlLoaderTest11')
                            ->has_relationship('mssql_loader_test12s');
                    } 'cross-schema relationship in multi-db_schema';
                }
            }

            SKIP: {
                # for ADO
                my $warn_handler = $SIG{__WARN__} || sub { warn @_ };
                local $SIG{__WARN__} = sub {
                    $warn_handler->(@_) unless $_[0] =~ /Changed database context/;
                };

                my $dbh = $schema->storage->dbh;

                try {
                    $dbh->do('USE master');
                    $dbh->do('CREATE DATABASE dbicsl_test1');
                }
                catch {
                    diag "no CREATE DATABASE privileges: '$_'";
                    skip "no CREATE DATABASE privileges", 26 * 2;
                };

                $dbh->do('CREATE DATABASE dbicsl_test2');

                $dbh->do('USE dbicsl_test1');

                $dbh->do(<<'EOF');
                    CREATE TABLE mssql_loader_test13 (
                        id INT IDENTITY PRIMARY KEY,
                        value VARCHAR(100)
                    )
EOF
                $dbh->do(<<'EOF');
                    CREATE TABLE mssql_loader_test14 (
                        id INT IDENTITY PRIMARY KEY,
                        value VARCHAR(100),
                        thirteen_id INTEGER REFERENCES mssql_loader_test13 (id),
                        CONSTRAINT loader_test14_uniq UNIQUE (thirteen_id)
                    )
EOF

                $dbh->do('USE dbicsl_test2');

                $dbh->do(<<'EOF');
                    CREATE TABLE mssql_loader_test14 (
                        pk INT IDENTITY PRIMARY KEY,
                        value VARCHAR(100),
                        thirteen_id INTEGER,
                        CONSTRAINT loader_test14_uniq UNIQUE (thirteen_id)
                    )
EOF

                $dbh->do(<<"EOF");
                    CREATE TABLE mssql_loader_test15 (
                        id INT IDENTITY PRIMARY KEY,
                        value VARCHAR(100)
                    )
EOF
                $dbh->do(<<"EOF");
                    CREATE TABLE mssql_loader_test16 (
                        id INT IDENTITY PRIMARY KEY,
                        value VARCHAR(100),
                        fifteen_id INTEGER UNIQUE REFERENCES mssql_loader_test15 (id)
                    )
EOF

                my $guard = Scope::Guard->new(\&cleanup_databases);

                foreach my $db_schema ({ dbicsl_test1 => '%', dbicsl_test2 => '%' }, { '%' => '%' }) {
                    lives_and {
                        my @warns;
                        local $SIG{__WARN__} = sub {
                            push @warns, $_[0] unless $_[0] =~ /\bcollides\b/;
                        };
     
                        make_schema_at(
                            'MSSQLMultiDatabase',
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
                    } "dumped schema for databases 'dbicsl_test1' and 'dbicsl_test2' with no warnings";

                    my $test_schema;

                    lives_and {
                        ok $test_schema = MSSQLMultiDatabase->connect(@$connect_info);
                    } 'connected test schema';

                    my ($rsrc, $rs, $row, $rel_info, %uniqs);

                    lives_and {
                        ok $rsrc = $test_schema->source('MssqlLoaderTest13');
                    } 'got source for table in database one';

                    is try { $rsrc->column_info('id')->{is_auto_increment} }, 1,
                        'column in database one';

                    is try { $rsrc->column_info('value')->{data_type} }, 'varchar',
                        'column in database one';

                    is try { $rsrc->column_info('value')->{size} }, 100,
                        'column in database one';

                    lives_and {
                        ok $rs = $test_schema->resultset('MssqlLoaderTest13');
                    } 'got resultset for table in database one';

                    lives_and {
                        ok $row = $rs->create({ value => 'foo' });
                    } 'executed SQL on table in database one';

                    $rel_info = try { $rsrc->relationship_info('mssql_loader_test14') };

                    is_deeply $rel_info->{cond}, {
                        'foreign.thirteen_id' => 'self.id'
                    }, 'relationship in database one';

                    is $rel_info->{attrs}{accessor}, 'single',
                        'relationship in database one';

                    is $rel_info->{attrs}{join_type}, 'LEFT',
                        'relationship in database one';

                    lives_and {
                        ok $rsrc = $test_schema->source('DbicslTest1MssqlLoaderTest14');
                    } 'got source for table in database one';

                    %uniqs = try { $rsrc->unique_constraints };

                    is keys %uniqs, 2,
                        'got unique and primary constraint in database one';

                    delete $uniqs{primary};

                    is_deeply ((values %uniqs)[0], ['thirteen_id'],
                        'correct unique constraint in database one');

                    lives_and {
                        ok $rsrc = $test_schema->source('MssqlLoaderTest15');
                    } 'got source for table in database two';

                    is try { $rsrc->column_info('id')->{is_auto_increment} }, 1,
                        'column in database two introspected correctly';

                    is try { $rsrc->column_info('value')->{data_type} }, 'varchar',
                        'column in database two introspected correctly';

                    is try { $rsrc->column_info('value')->{size} }, 100,
                        'column in database two introspected correctly';

                    lives_and {
                        ok $rs = $test_schema->resultset('MssqlLoaderTest15');
                    } 'got resultset for table in database two';

                    lives_and {
                        ok $row = $rs->create({ value => 'foo' });
                    } 'executed SQL on table in database two';

                    $rel_info = try { $rsrc->relationship_info('mssql_loader_test16') };

                    is_deeply $rel_info->{cond}, {
                        'foreign.fifteen_id' => 'self.id'
                    }, 'relationship in database two';

                    is $rel_info->{attrs}{accessor}, 'single',
                        'relationship in database two';

                    is $rel_info->{attrs}{join_type}, 'LEFT',
                        'relationship in database two';

                    lives_and {
                        ok $rsrc = $test_schema->source('MssqlLoaderTest16');
                    } 'got source for table in database two';

                    %uniqs = try { $rsrc->unique_constraints };

                    is keys %uniqs, 2,
                        'got unique and primary constraint in database two';

                    delete $uniqs{primary};

                    is_deeply ((values %uniqs)[0], ['fifteen_id'],
                        'correct unique constraint in database two');
                }
            }
        },
    },
);

$tester->run_tests();

sub cleanup_schemas {
    return if $ENV{SCHEMA_LOADER_TESTS_NOCLEANUP};

    # switch back to default database
    $schema->storage->disconnect;
    my $dbh = $schema->storage->dbh;

    foreach my $table ('[dbicsl-test].mssql_loader_test12',
                       '[dbicsl.test].mssql_loader_test11',
                       '[dbicsl.test].mssql_loader_test10',
                       '[dbicsl.test].mssql_loader_test9',
                       '[dbicsl-test].mssql_loader_test9',
                       '[dbicsl-test].mssql_loader_test8') {
        try {
            $dbh->do("DROP TABLE $table");
        }
        catch {
            diag "Error dropping table: $_";
        };
    }

    foreach my $db_schema (qw/dbicsl-test dbicsl.test/) {
        try {
            $dbh->do(qq{DROP SCHEMA [$db_schema]});
        }
        catch {
            diag "Error dropping test schema $db_schema: $_";
        };
    }

    rmtree EXTRA_DUMP_DIR;
}

sub cleanup_databases {
    return if $ENV{SCHEMA_LOADER_TESTS_NOCLEANUP};

    # for ADO
    my $warn_handler = $SIG{__WARN__} || sub { warn @_ };
    local $SIG{__WARN__} = sub {
        $warn_handler->(@_) unless $_[0] =~ /Changed database context/;
    };

    my $dbh = $schema->storage->dbh;

    $dbh->do('USE dbicsl_test1');

    foreach my $table ('mssql_loader_test14',
                       'mssql_loader_test13') {
        try {
            $dbh->do("DROP TABLE $table");
        }
        catch {
            diag "Error dropping table: $_";
        };
    }

    $dbh->do('USE dbicsl_test2');

    foreach my $table ('mssql_loader_test16',
                       'mssql_loader_test15',
                       'mssql_loader_test14') {
        try {
            $dbh->do("DROP TABLE $table");
        }
        catch {
            diag "Error dropping table: $_";
        };
    }

    $dbh->do('USE master');

    foreach my $database (qw/dbicsl_test1 dbicsl_test2/) {
        try {
            $dbh->do(qq{DROP DATABASE $database});
        }
        catch {
            diag "Error dropping test database '$database': $_";
        };
    }

    rmtree EXTRA_DUMP_DIR;
}
# vim:et sts=4 sw=4 tw=0:
