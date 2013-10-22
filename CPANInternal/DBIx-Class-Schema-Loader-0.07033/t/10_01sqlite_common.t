use strict;
use Test::More;
use lib qw(t/lib);
use dbixcsl_common_tests;
use dbixcsl_test_dir qw/$tdir/;

eval { require DBD::SQLite };
my $class = $@ ? 'SQLite2' : 'SQLite';

my $tester = dbixcsl_common_tests->new(
    vendor          => 'SQLite',
    auto_inc_pk     => 'INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT',
    dsn             => "dbi:$class:dbname=$tdir/sqlite_test",
    user            => '',
    password        => '',
    connect_info_opts => {
        on_connect_do => [ 'PRAGMA foreign_keys = ON', 'PRAGMA synchronous = OFF', ]
    },
    loader_options  => { preserve_case => 1 },
    default_is_deferrable => 0,
    default_on_clause => 'NO ACTION',
    data_types  => {
        # SQLite ignores data types aside from INTEGER pks.
        # We just test that they roundtrip sanely.
        #
        # Numeric types
        'smallint'    => { data_type => 'smallint' },
        'int'         => { data_type => 'int' },
        'integer'     => { data_type => 'integer' },

        # test that type name is lowercased
        'INTEGER'     => { data_type => 'integer' },

        'bigint'      => { data_type => 'bigint' },
        'float'       => { data_type => 'float' },
        'double precision' =>
                         { data_type => 'double precision' },
        'real'        => { data_type => 'real' },

        'float(2)'    => { data_type => 'float', size => 2 },
        'float(7)'    => { data_type => 'float', size => 7 },

        'decimal'     => { data_type => 'decimal' },
        'dec'         => { data_type => 'dec' },
        'numeric'     => { data_type => 'numeric' },

        'decimal(3)'   => { data_type => 'decimal', size => 3 },
        'numeric(3)'   => { data_type => 'numeric', size => 3 },

        'decimal(3,3)' => { data_type => 'decimal', size => [3,3] },
        'dec(3,3)'     => { data_type => 'dec', size => [3,3] },
        'numeric(3,3)' => { data_type => 'numeric', size => [3,3] },

        # Date and Time Types
        'date'        => { data_type => 'date' },
        'timestamp DEFAULT CURRENT_TIMESTAMP'
                      => { data_type => 'timestamp', default_value => \'current_timestamp' },
        'time'        => { data_type => 'time' },

        # String Types
        'char'         => { data_type => 'char' },
        'char(11)'     => { data_type => 'char',    size => 11 },
        'varchar(20)'  => { data_type => 'varchar', size => 20 },
    },
    extra           => {
        create => [
            # 'sqlite_' is reserved, so we use 'extra_'
            q{
                CREATE TABLE "extra_loader_test1" (
                    "id" NOT NULL PRIMARY KEY,
                    "value" TEXT UNIQUE NOT NULL
                )
            },
            q{
                CREATE TABLE extra_loader_test2 (
                    event_id INTEGER PRIMARY KEY
                )
            },
            q{
                CREATE TABLE extra_loader_test3 (
                    person_id INTEGER PRIMARY KEY
                )
            },
            # Wordy, newline-heavy SQL
            q{
                CREATE TABLE extra_loader_test4 (
                    event_id INTEGER NOT NULL
                        CONSTRAINT fk_event_id
                        REFERENCES extra_loader_test2(event_id),
                    person_id INTEGER NOT NULL
                        CONSTRAINT fk_person_id
                        REFERENCES extra_loader_test3 (person_id),
                    PRIMARY KEY (event_id, person_id)
                )
            },
            # make sure views are picked up
            q{
                CREATE VIEW extra_loader_test5 AS SELECT * FROM extra_loader_test4
            },
            # Compound primary keys can't be autoinc in the DBIC sense
            q{
                CREATE TABLE extra_loader_test6 (
                  id1 INTEGER,
                  id2 INTEGER,
                  value INTEGER,
                  PRIMARY KEY (id1, id2)
                )
            },
            q{
                CREATE TABLE extra_loader_test7 (
                  id1 INTEGER,
                  id2 TEXT,
                  value DECIMAL,
                  PRIMARY KEY (id1, id2)
                )
            },
            q{
                create table extra_loader_test8 (
                    id integer primary key
                )
            },
            q{
                create table extra_loader_test9 (
                    id integer primary key,
                    eight_id int,
                    foreign key (eight_id) references extra_loader_test8(id)
                        on delete restrict on update set null deferrable
                )
            },
            # test inline constraint
            q{
                create table extra_loader_test10 (
                    id integer primary key,
                    eight_id int references extra_loader_test8(id) on delete restrict on update set null deferrable
                )
            },
        ],
        pre_drop_ddl => [ 'DROP VIEW extra_loader_test5' ],
        drop  => [ qw/extra_loader_test1 extra_loader_test2 extra_loader_test3
                      extra_loader_test4 extra_loader_test6 extra_loader_test7
                      extra_loader_test8 extra_loader_test9 extra_loader_test10 / ],
        count => 19,
        run   => sub {
            my ($schema, $monikers, $classes) = @_;

            ok ((my $rs = $schema->resultset($monikers->{extra_loader_test1})),
                'resultset for quoted table');

            ok ((my $source = $rs->result_source), 'source');

            is_deeply [ $source->columns ], [ qw/id value/ ],
                'retrieved quoted column names from quoted table';

            ok ((exists $source->column_info('value')->{is_nullable}),
                'is_nullable exists');

            is $source->column_info('value')->{is_nullable}, 0,
                'is_nullable is set correctly';

            ok (($source = $schema->source($monikers->{extra_loader_test4})),
                'verbose table');

            is_deeply [ $source->primary_columns ], [ qw/event_id person_id/ ],
                'composite primary key';

            is ($source->relationships, 2,
                '2 foreign key constraints found');

            # test that columns for views are picked up
            is $schema->resultset($monikers->{extra_loader_test5})->result_source->column_info('person_id')->{data_type}, 'integer',
                'columns for views are introspected';

            isnt $schema->resultset($monikers->{extra_loader_test6})->result_source->column_info('id1')->{is_auto_increment}, 1,
                q{two integer PKs don't get marked autoinc};

            isnt $schema->resultset($monikers->{extra_loader_test7})->result_source->column_info('id1')->{is_auto_increment}, 1,
                q{composite integer PK with non-integer PK doesn't get marked autoinc};

            # test on delete/update fk clause introspection
            ok ((my $rel_info = $schema->source('ExtraLoaderTest9')->relationship_info('eight')),
                'got rel info');

            is $rel_info->{attrs}{on_delete}, 'RESTRICT',
                'ON DELETE clause introspected correctly';

            is $rel_info->{attrs}{on_update}, 'SET NULL',
                'ON UPDATE clause introspected correctly';

            is $rel_info->{attrs}{is_deferrable}, 1,
                'DEFERRABLE clause introspected correctly';

            ok (($rel_info = $schema->source('ExtraLoaderTest10')->relationship_info('eight')),
                'got rel info');

            is $rel_info->{attrs}{on_delete}, 'RESTRICT',
                'ON DELETE clause introspected correctly for inline FK';

            is $rel_info->{attrs}{on_update}, 'SET NULL',
                'ON UPDATE clause introspected correctly for inline FK';

            is $rel_info->{attrs}{is_deferrable}, 1,
                'DEFERRABLE clause introspected correctly for inline FK';
        },
    },
);

$tester->run_tests();

END {
    unlink "$tdir/sqlite_test" unless $ENV{SCHEMA_LOADER_TESTS_NOCLEANUP};
}
