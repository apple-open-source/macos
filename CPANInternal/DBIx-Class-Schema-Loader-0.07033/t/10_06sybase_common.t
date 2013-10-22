use strict;
use warnings;
use Test::More;
use Test::Exception;
use Try::Tiny;
use File::Path 'rmtree';
use DBIx::Class::Schema::Loader 'make_schema_at';
use namespace::clean;
use DBI ();

use lib qw(t/lib);

use dbixcsl_common_tests ();
use dbixcsl_test_dir '$tdir';

use constant EXTRA_DUMP_DIR => "$tdir/sybase_extra_dump";

my $dsn      = $ENV{DBICTEST_SYBASE_DSN} || '';
my $user     = $ENV{DBICTEST_SYBASE_USER} || '';
my $password = $ENV{DBICTEST_SYBASE_PASS} || '';

BEGIN { $ENV{DBIC_SYBASE_FREETDS_NOWARN} = 1 }

my ($schema, $databases_created); # for cleanup in END for extra tests

my $tester = dbixcsl_common_tests->new(
    vendor      => 'sybase',
    auto_inc_pk => 'INTEGER IDENTITY NOT NULL PRIMARY KEY',
    default_function     => 'getdate()',
    default_function_def => 'AS getdate()',
    dsn         => $dsn,
    user        => $user,
    password    => $password,
    data_types  => {
        # http://ispirer.com/wiki/sqlways/sybase/data-types
        #
        # Numeric Types
        'integer identity' => { data_type => 'integer', is_auto_increment => 1 },
        int      => { data_type => 'integer' },
        integer  => { data_type => 'integer' },
        bigint   => { data_type => 'bigint' },
        smallint => { data_type => 'smallint' },
        tinyint  => { data_type => 'tinyint' },
        'double precision' => { data_type => 'double precision' },
        real           => { data_type => 'real' },
        float          => { data_type => 'double precision' },
        'float(14)'    => { data_type => 'real' },
        'float(15)'    => { data_type => 'real' },
        'float(16)'    => { data_type => 'double precision' },
        'float(48)'    => { data_type => 'double precision' },
        'numeric(6,3)' => { data_type => 'numeric', size => [6,3] },
        'decimal(6,3)' => { data_type => 'numeric', size => [6,3] },
        numeric        => { data_type => 'numeric' },
        decimal        => { data_type => 'numeric' },
        bit            => { data_type => 'bit' },

        # Money Types
        money          => { data_type => 'money' },
        smallmoney     => { data_type => 'smallmoney' },

        # Computed Column
        'AS getdate()'     => { data_type => undef, inflate_datetime => 1, default_value => \'getdate()' },

        # Blob Types
        text     => { data_type => 'text' },
        unitext  => { data_type => 'unitext' },
        image    => { data_type => 'image' },

        # DateTime Types
        date     => { data_type => 'date' },
        time     => { data_type => 'time' },
        datetime => { data_type => 'datetime' },
        smalldatetime  => { data_type => 'smalldatetime' },

        # Timestamp column
        timestamp      => { data_type => 'timestamp', inflate_datetime => 0 },

        # String Types
        'char'         => { data_type => 'char', size => 1 },
        'char(2)'      => { data_type => 'char', size => 2 },
        'nchar'        => { data_type => 'nchar', size => 1 },
        'nchar(2)'     => { data_type => 'nchar', size => 2 },
        'unichar(2)'   => { data_type => 'unichar', size => 2 },
        'varchar(2)'   => { data_type => 'varchar', size => 2 },
        'nvarchar(2)'  => { data_type => 'nvarchar', size => 2 },
        'univarchar(2)' => { data_type => 'univarchar', size => 2 },

        # Binary Types
        'binary'       => { data_type => 'binary', size => 1 },
        'binary(2)'    => { data_type => 'binary', size => 2 },
        'varbinary(2)' => { data_type => 'varbinary', size => 2 },
    },
    # test that named constraints aren't picked up as tables (I can't reproduce this on my machine)
    failtrigger_warnings => [ qr/^Bad table or view 'sybase_loader_test2_ref_slt1'/ ],
    extra => {
        create => [
            q{
                CREATE TABLE sybase_loader_test1 (
                    id int identity primary key
                )
            },
            q{
                CREATE TABLE sybase_loader_test2 (
                    id int identity primary key,
                    sybase_loader_test1_id int,
                    CONSTRAINT sybase_loader_test2_ref_slt1 FOREIGN KEY (sybase_loader_test1_id) REFERENCES sybase_loader_test1 (id)
                )
            },
        ],
        drop => [ qw/sybase_loader_test1 sybase_loader_test2/ ],
        count => 30 * 4,
        run => sub {
            $schema = shift;

            SKIP: {
                my $dbh = $schema->storage->dbh;

                try {
                    $dbh->do('USE master');
                }
                catch {
                    skip "these tests require the sysadmin role", 30 * 4;
                };

                try {
                    $dbh->do('CREATE DATABASE [dbicsl_test1]');
                    $dbh->do('CREATE DATABASE [dbicsl_test2]');
                }
                catch {
                    skip "cannot create databases: $_", 30 * 4;
                };

                try {
                    my $warn_handler = $SIG{__WARN__} || sub { warn @_ };
                    local $SIG{__WARN__} = sub {
                        $warn_handler->(@_)
                            unless $_[0] =~ /^Password correctly set\.$|^Account unlocked\.$|^New login created\.$|^New user added\.$/;
                    };

                    $dbh->do("sp_addlogin dbicsl_user1, dbicsl, [dbicsl_test1]");
                    $dbh->do("sp_addlogin dbicsl_user2, dbicsl, [dbicsl_test2]");

                    $dbh->do("USE [dbicsl_test1]");
                    $dbh->do("sp_adduser dbicsl_user1");
                    $dbh->do("sp_adduser dbicsl_user2");
                    $dbh->do("GRANT ALL TO dbicsl_user1");
                    $dbh->do("GRANT ALL TO dbicsl_user2");

                    $dbh->do("USE [dbicsl_test2]");
                    $dbh->do("sp_adduser dbicsl_user2");
                    $dbh->do("sp_adduser dbicsl_user1");
                    $dbh->do("GRANT ALL TO dbicsl_user2");
                    $dbh->do("GRANT ALL TO dbicsl_user1");
                }
                catch {
                    skip "cannot add logins: $_", 30 * 4;
                };

                my ($dbh1, $dbh2);
                {
                    my $warn_handler = $SIG{__WARN__} || sub { warn @_ };
                    local $SIG{__WARN__} = sub {
                        $warn_handler->(@_) unless $_[0] =~ /can't change context/;
                    };

                    $dbh1 = DBI->connect($dsn, 'dbicsl_user1', 'dbicsl', {
                        RaiseError => 1,
                        PrintError => 0,
                    });
                    $dbh1->do('USE [dbicsl_test1]');

                    $dbh2 = DBI->connect($dsn, 'dbicsl_user2', 'dbicsl', {
                        RaiseError => 1,
                        PrintError => 0,
                    });
                    $dbh2->do('USE [dbicsl_test2]');
                }

                $dbh1->do(<<"EOF");
                    CREATE TABLE sybase_loader_test4 (
                        id INT IDENTITY PRIMARY KEY,
                        value VARCHAR(100) NULL
                    )
EOF
                $dbh1->do('GRANT ALL ON sybase_loader_test4 TO dbicsl_user2');
                $dbh1->do(<<"EOF");
                    CREATE TABLE sybase_loader_test5 (
                        id INT IDENTITY PRIMARY KEY,
                        value VARCHAR(100) NULL,
                        four_id INTEGER,
                        CONSTRAINT loader_test5_uniq UNIQUE (four_id),
                        FOREIGN KEY (four_id) REFERENCES sybase_loader_test4 (id)
                    )
EOF
                $dbh2->do(<<"EOF");
                    CREATE TABLE sybase_loader_test5 (
                        pk INT IDENTITY PRIMARY KEY,
                        value VARCHAR(100) NULL,
                        four_id INTEGER,
                        CONSTRAINT loader_test5_uniq UNIQUE (four_id),
                        FOREIGN KEY (four_id) REFERENCES [dbicsl_test1].dbicsl_user1.sybase_loader_test4 (id)
                    )
EOF
                $dbh2->do(<<"EOF");
                    CREATE TABLE sybase_loader_test6 (
                        id INT IDENTITY PRIMARY KEY,
                        value VARCHAR(100) NULL,
                        sybase_loader_test4_id INTEGER NULL,
                        FOREIGN KEY (sybase_loader_test4_id) REFERENCES [dbicsl_test1].dbicsl_user1.sybase_loader_test4 (id)
                    )
EOF
                $dbh2->do(<<"EOF");
                    CREATE TABLE sybase_loader_test7 (
                        id INT IDENTITY PRIMARY KEY,
                        value VARCHAR(100) NULL,
                        six_id INTEGER UNIQUE,
                        FOREIGN KEY (six_id) REFERENCES sybase_loader_test6 (id)
                    )
EOF
                $dbh2->do('GRANT ALL ON sybase_loader_test7 TO dbicsl_user1');
                $dbh1->do(<<"EOF");
                    CREATE TABLE sybase_loader_test8 (
                        id INT IDENTITY PRIMARY KEY,
                        value VARCHAR(100) NULL,
                        sybase_loader_test7_id INTEGER,
                        FOREIGN KEY (sybase_loader_test7_id) REFERENCES [dbicsl_test2].dbicsl_user2.sybase_loader_test7 (id)
                    )
EOF

                $databases_created = 1;

                foreach my $databases (['dbicsl_test1', 'dbicsl_test2'], '%') {
                    foreach my $owners ([qw/dbicsl_user1 dbicsl_user2/], '%') {
                        lives_and {
                            rmtree EXTRA_DUMP_DIR;

                            my @warns;
                            local $SIG{__WARN__} = sub {
                                push @warns, $_[0] unless $_[0] =~ /\bcollides\b/
                                    || $_[0] =~ /can't change context/;
                            };

                            my $database = $databases;

                            $database = [ $database ] unless ref $database;

                            my $db_schema = {};

                            foreach my $db (@$database) {
                                $db_schema->{$db} = $owners;
                            }

                            make_schema_at(
                                'SybaseMultiSchema',
                                {
                                    naming => 'current',
                                    db_schema => $db_schema,
                                    dump_directory => EXTRA_DUMP_DIR,
                                    quiet => 1,
                                },
                                [ $dsn, $user, $password ],
                            );

                            SybaseMultiSchema->storage->disconnect;

                            diag join "\n", @warns if @warns;

                            is @warns, 0;
                        } 'dumped schema for "dbicsl_test1" and "dbicsl_test2" databases with no warnings';

                        my ($test_schema, $rsrc, $rs, $row, %uniqs, $rel_info);

                        lives_and {
                            ok $test_schema = SybaseMultiSchema->connect($dsn, $user, $password);
                        } 'connected test schema';

                        lives_and {
                            ok $rsrc = $test_schema->source('SybaseLoaderTest4');
                        } 'got source for table in database one';

                        is try { $rsrc->column_info('id')->{is_auto_increment} }, 1,
                            'column in database one';

                        is try { $rsrc->column_info('value')->{data_type} }, 'varchar',
                            'column in database one';

                        is try { $rsrc->column_info('value')->{size} }, 100,
                            'column in database one';

                        lives_and {
                            ok $rs = $test_schema->resultset('SybaseLoaderTest4');
                        } 'got resultset for table in database one';

                        lives_and {
                            ok $row = $rs->create({ value => 'foo' });
                        } 'executed SQL on table in database one';

                        $rel_info = try { $rsrc->relationship_info('dbicsl_test1_sybase_loader_test5') };

                        is_deeply $rel_info->{cond}, {
                            'foreign.four_id' => 'self.id'
                        }, 'relationship in database one';

                        is $rel_info->{attrs}{accessor}, 'single',
                            'relationship in database one';

                        is $rel_info->{attrs}{join_type}, 'LEFT',
                            'relationship in database one';

                        lives_and {
                            ok $rsrc = $test_schema->source('DbicslTest1SybaseLoaderTest5');
                        } 'got source for table in database one';

                        %uniqs = try { $rsrc->unique_constraints };

                        is keys %uniqs, 2,
                            'got unique and primary constraint in database one';

                        delete $uniqs{primary};

                        is_deeply ((values %uniqs)[0], ['four_id'],
                            'correct unique constraint in database one');

                        lives_and {
                            ok $rsrc = $test_schema->source('SybaseLoaderTest6');
                        } 'got source for table in database two';

                        is try { $rsrc->column_info('id')->{is_auto_increment} }, 1,
                            'column in database two introspected correctly';

                        is try { $rsrc->column_info('value')->{data_type} }, 'varchar',
                            'column in database two introspected correctly';

                        is try { $rsrc->column_info('value')->{size} }, 100,
                            'column in database two introspected correctly';

                        lives_and {
                            ok $rs = $test_schema->resultset('SybaseLoaderTest6');
                        } 'got resultset for table in database two';

                        lives_and {
                            ok $row = $rs->create({ value => 'foo' });
                        } 'executed SQL on table in database two';

                        $rel_info = try { $rsrc->relationship_info('sybase_loader_test7') };

                        is_deeply $rel_info->{cond}, {
                            'foreign.six_id' => 'self.id'
                        }, 'relationship in database two';

                        is $rel_info->{attrs}{accessor}, 'single',
                            'relationship in database two';

                        is $rel_info->{attrs}{join_type}, 'LEFT',
                            'relationship in database two';

                        lives_and {
                            ok $rsrc = $test_schema->source('SybaseLoaderTest7');
                        } 'got source for table in database two';

                        %uniqs = try { $rsrc->unique_constraints };

                        is keys %uniqs, 2,
                            'got unique and primary constraint in database two';

                        delete $uniqs{primary};

                        is_deeply ((values %uniqs)[0], ['six_id'],
                            'correct unique constraint in database two');

                        lives_and {
                            ok $test_schema->source('SybaseLoaderTest6')
                                ->has_relationship('sybase_loader_test4');
                        } 'cross-database relationship in multi database schema';

                        lives_and {
                            ok $test_schema->source('SybaseLoaderTest4')
                                ->has_relationship('sybase_loader_test6s');
                        } 'cross-database relationship in multi database schema';

                        lives_and {
                            ok $test_schema->source('SybaseLoaderTest8')
                                ->has_relationship('sybase_loader_test7');
                        } 'cross-database relationship in multi database schema';

                        lives_and {
                            ok $test_schema->source('SybaseLoaderTest7')
                                ->has_relationship('sybase_loader_test8s');
                        } 'cross-database relationship in multi database schema';
                    }
                }
            }
        },
    },
);

if( !$dsn || !$user ) {
    $tester->skip_tests('You need to set the DBICTEST_SYBASE_DSN, _USER, and _PASS environment variables');
}
else {
    $tester->run_tests();
}

END {
    if (not $ENV{SCHEMA_LOADER_TESTS_NOCLEANUP}) {
        rmtree EXTRA_DUMP_DIR;

        if ($databases_created) {
            my $dbh = $schema->storage->dbh;

            $dbh->do('USE master');

            local $dbh->{FetchHashKeyName} = 'NAME_lc';

            my $sth = $dbh->prepare('sp_who');
            $sth->execute;

            while (my $row = $sth->fetchrow_hashref) {
                if ($row->{dbname} =~ /^dbicsl_test[12]\z/) {
                    $dbh->do("kill $row->{spid}");
                }
            }

            foreach my $table ('[dbicsl_test1].dbicsl_user1.sybase_loader_test8',
                               '[dbicsl_test2].dbicsl_user2.sybase_loader_test7',
                               '[dbicsl_test2].dbicsl_user2.sybase_loader_test6',
                               '[dbicsl_test2].dbicsl_user2.sybase_loader_test5',
                               '[dbicsl_test1].dbicsl_user1.sybase_loader_test5',
                               '[dbicsl_test1].dbicsl_user1.sybase_loader_test4') {
                try {
                    $dbh->do("DROP TABLE $table");
                }
                catch {
                    diag "Error dropping table $table: $_";
                };
            }

            foreach my $db (qw/dbicsl_test1 dbicsl_test2/) {
                try {
                    $dbh->do("DROP DATABASE [$db]");
                }
                catch {
                    diag "Error dropping test database $db: $_";
                };
            }

            foreach my $login (qw/dbicsl_user1 dbicsl_user2/) {
                try {
                    my $warn_handler = $SIG{__WARN__} || sub { warn @_ };
                    local $SIG{__WARN__} = sub {
                        $warn_handler->(@_)
                            unless $_[0] =~ /^Account locked\.$|^Login dropped\.$/;
                    };

                    $dbh->do("sp_droplogin $login");
                }
                catch {
                    diag "Error dropping login $login: $_"
                        unless /Incorrect syntax/;
                };
            }
        }
    }
}
# vim:et sts=4 sw=4 tw=0:
