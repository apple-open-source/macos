use strict;
use warnings;
use utf8;
use DBIx::Class::Schema::Loader 'make_schema_at';
use DBIx::Class::Schema::Loader::Utils qw/no_warnings slurp_file/;
use Test::More;
use Test::Exception;
use Try::Tiny;
use File::Path 'rmtree';
use namespace::clean;

use lib qw(t/lib);
use dbixcsl_common_tests ();
use dbixcsl_test_dir '$tdir';

use constant EXTRA_DUMP_DIR => "$tdir/pg_extra_dump";

my $dsn      = $ENV{DBICTEST_PG_DSN} || '';
my $user     = $ENV{DBICTEST_PG_USER} || '';
my $password = $ENV{DBICTEST_PG_PASS} || '';

my $tester = dbixcsl_common_tests->new(
    vendor      => 'Pg',
    auto_inc_pk => 'SERIAL NOT NULL PRIMARY KEY',
    dsn         => $dsn,
    user        => $user,
    password    => $password,
    loader_options  => { preserve_case => 1 },
    connect_info_opts => {
        pg_enable_utf8 => 1,
        on_connect_do  => [ 'SET client_min_messages=WARNING' ],
    },
    quote_char  => '"',
    default_is_deferrable => 0,
    default_on_clause => 'NO ACTION',
    data_types  => {
        # http://www.postgresql.org/docs/7.4/interactive/datatype.html
        #
        # Numeric Types
	boolean     => { data_type => 'boolean' },
	bool        => { data_type => 'boolean' },
        'bool default false'
                    => { data_type => 'boolean', default_value => \'false' },
        'bool default true'
                    => { data_type => 'boolean', default_value => \'true' },
        'bool default 0::bool'
                    => { data_type => 'boolean', default_value => \'false' },
        'bool default 1::bool'
                    => { data_type => 'boolean', default_value => \'true' },

	bigint      => { data_type => 'bigint' },
	int8        => { data_type => 'bigint' },
	bigserial   => { data_type => 'bigint', is_auto_increment => 1 },
	serial8     => { data_type => 'bigint', is_auto_increment => 1 },
	integer     => { data_type => 'integer' },
	int         => { data_type => 'integer' },
	int4        => { data_type => 'integer' },
	serial      => { data_type => 'integer', is_auto_increment => 1 },
	serial4     => { data_type => 'integer', is_auto_increment => 1 },
	smallint    => { data_type => 'smallint' },
	int2        => { data_type => 'smallint' },

	money       => { data_type => 'money' },

	'double precision' => { data_type => 'double precision' },
	float8             => { data_type => 'double precision' },
	real               => { data_type => 'real' },
	float4             => { data_type => 'real' },
        'float(24)'        => { data_type => 'real' },
        'float(25)'        => { data_type => 'double precision' },
        'float(53)'        => { data_type => 'double precision' },
        float              => { data_type => 'double precision' },

        numeric        => { data_type => 'numeric' },
        decimal        => { data_type => 'numeric' },
	'numeric(6,3)' => { data_type => 'numeric', size => [6,3] },
	'decimal(6,3)' => { data_type => 'numeric', size => [6,3] },

        # Bit String Types
	'bit varying(2)' => { data_type => 'varbit', size => 2 },
	'varbit(2)'      => { data_type => 'varbit', size => 2 },
	'varbit'         => { data_type => 'varbit' },
	bit              => { data_type => 'bit', size => 1 },
	'bit(3)'         => { data_type => 'bit', size => 3 },

        # Network Types
	inet    => { data_type => 'inet' },
	cidr    => { data_type => 'cidr' },
	macaddr => { data_type => 'macaddr' },

        # Geometric Types
	point   => { data_type => 'point' },
	line    => { data_type => 'line' },
	lseg    => { data_type => 'lseg' },
	box     => { data_type => 'box' },
	path    => { data_type => 'path' },
	polygon => { data_type => 'polygon' },
	circle  => { data_type => 'circle' },

        # Character Types
	'character varying(2)'           => { data_type => 'varchar', size => 2 },
	'varchar(2)'                     => { data_type => 'varchar', size => 2 },
	'character(2)'                   => { data_type => 'char', size => 2 },
	'char(2)'                        => { data_type => 'char', size => 2 },
        # check that default null is correctly rewritten
        'char(3) default null'           => { data_type => 'char', size => 3,
                                              default_value => \'null' },
	'character'                      => { data_type => 'char', size => 1 },
	'char'                           => { data_type => 'char', size => 1 },
	text                             => { data_type => 'text' },
        # varchar with no size has unlimited size, we rewrite to 'text'
	varchar                          => { data_type => 'text',
                                              original => { data_type => 'varchar' } },
        # check default null again (to make sure ref is safe)
        'varchar(3) default null'        => { data_type => 'varchar', size => 3,
                                              default_value => \'null' },

        # Datetime Types
	date                             => { data_type => 'date' },
	interval                         => { data_type => 'interval' },
	'interval(2)'                    => { data_type => 'interval', size => 2 },
	time                             => { data_type => 'time' },
	'time(2)'                        => { data_type => 'time', size => 2 },
	'time without time zone'         => { data_type => 'time' },
	'time(2) without time zone'      => { data_type => 'time', size => 2 },
	'time with time zone'            => { data_type => 'time with time zone' },
	'time(2) with time zone'         => { data_type => 'time with time zone', size => 2 },
	timestamp                        => { data_type => 'timestamp' },
        'timestamp default now()'
                                         => { data_type => 'timestamp', default_value => \'current_timestamp',
                                              original => { default_value => \'now()' } },
	'timestamp(2)'                   => { data_type => 'timestamp', size => 2 },
	'timestamp without time zone'    => { data_type => 'timestamp' },
	'timestamp(2) without time zone' => { data_type => 'timestamp', size => 2 },

	'timestamp with time zone'       => { data_type => 'timestamp with time zone' },
	'timestamp(2) with time zone'    => { data_type => 'timestamp with time zone', size => 2 },

        # Blob Types
	bytea => { data_type => 'bytea' },

        # Enum Types
        pg_loader_test_enum => { data_type => 'enum', extra => { custom_type_name => 'pg_loader_test_enum',
                                                                 list => [ qw/foo bar baz/] } },
    },
    pre_create => [
        q{
            CREATE TYPE pg_loader_test_enum AS ENUM (
                'foo', 'bar', 'baz'
            )
        },
    ],
    extra       => {
        create => [
            q{
                CREATE SCHEMA dbicsl_test
            },
            q{
                CREATE SEQUENCE dbicsl_test.myseq
            },
            q{
                CREATE TABLE pg_loader_test1 (
                    id INTEGER NOT NULL DEFAULT nextval('dbicsl_test.myseq') PRIMARY KEY,
                    value VARCHAR(100)
                )
            },
            qq{
                COMMENT ON TABLE pg_loader_test1 IS 'The\15\12Table ∑'
            },
            qq{
                COMMENT ON COLUMN pg_loader_test1.value IS 'The\15\12Column'
            },
            q{
                CREATE TABLE pg_loader_test2 (
                    id SERIAL PRIMARY KEY,
                    value VARCHAR(100)
                )
            },
            q{
                COMMENT ON TABLE pg_loader_test2 IS 'very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very long comment'
            },
            q{
                CREATE SCHEMA "dbicsl-test"
            },
            q{
                CREATE TABLE "dbicsl-test".pg_loader_test4 (
                    id SERIAL PRIMARY KEY,
                    value VARCHAR(100)
                )
            },
            q{
                CREATE TABLE "dbicsl-test".pg_loader_test5 (
                    id SERIAL PRIMARY KEY,
                    value VARCHAR(100),
                    four_id INTEGER REFERENCES "dbicsl-test".pg_loader_test4 (id),
                    CONSTRAINT loader_test5_uniq UNIQUE (four_id)
                )
            },
            q{
                CREATE SCHEMA "dbicsl.test"
            },
            q{
                CREATE TABLE "dbicsl.test".pg_loader_test5 (
                    pk SERIAL PRIMARY KEY,
                    value VARCHAR(100),
                    four_id INTEGER REFERENCES "dbicsl-test".pg_loader_test4 (id),
                    CONSTRAINT loader_test5_uniq UNIQUE (four_id)
                )
            },
            q{
                CREATE TABLE "dbicsl.test".pg_loader_test6 (
                    id SERIAL PRIMARY KEY,
                    value VARCHAR(100),
                    pg_loader_test4_id INTEGER REFERENCES "dbicsl-test".pg_loader_test4 (id)
                )
            },
            q{
                CREATE TABLE "dbicsl.test".pg_loader_test7 (
                    id SERIAL PRIMARY KEY,
                    value VARCHAR(100),
                    six_id INTEGER UNIQUE REFERENCES "dbicsl.test".pg_loader_test6 (id)
                )
            },
            q{
                CREATE TABLE "dbicsl-test".pg_loader_test8 (
                    id SERIAL PRIMARY KEY,
                    value VARCHAR(100),
                    pg_loader_test7_id INTEGER REFERENCES "dbicsl.test".pg_loader_test7 (id)
                )
            },
            # 4 through 8 are used for the multi-schema tests
            q{
                create table pg_loader_test9 (
                    id bigserial primary key
                )
            },
            q{
                create table pg_loader_test10 (
                    id bigserial primary key,
                    nine_id int,
                    foreign key (nine_id) references pg_loader_test9(id)
                        on delete restrict on update set null deferrable
                )
            },
        ],
        pre_drop_ddl => [
            'DROP SCHEMA dbicsl_test CASCADE',
            'DROP SCHEMA "dbicsl-test" CASCADE',
            'DROP SCHEMA "dbicsl.test" CASCADE',
            'DROP TYPE pg_loader_test_enum',
        ],
        drop  => [ qw/pg_loader_test1 pg_loader_test2 pg_loader_test9 pg_loader_test10/ ],
        count => 8 + 30 * 2,
        run   => sub {
            my ($schema, $monikers, $classes) = @_;

            is $schema->source($monikers->{pg_loader_test1})->column_info('id')->{sequence},
                'dbicsl_test.myseq',
                'qualified sequence detected';

            my $class    = $classes->{pg_loader_test1};
            my $filename = $schema->loader->get_dump_filename($class);

            my $code = slurp_file $filename;

            like $code, qr/^=head1 NAME\n\n^$class - The\nTable ∑\n\n^=cut\n/m,
                'table comment';

            like $code, qr/^=head2 value\n\n(.+:.+\n)+\nThe\nColumn\n\n/m,
                'column comment and attrs';

            $class    = $classes->{pg_loader_test2};
            $filename = $schema->loader->get_dump_filename($class);

            $code = slurp_file $filename;

            like $code, qr/^=head1 NAME\n\n^$class\n\n=head1 DESCRIPTION\n\n^very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very long comment\n\n^=cut\n/m,
                'long table comment is in DESCRIPTION';

            # test on delete/update fk clause introspection
            ok ((my $rel_info = $schema->source('PgLoaderTest10')->relationship_info('nine')),
                'got rel info');

            is $rel_info->{attrs}{on_delete}, 'RESTRICT',
                'ON DELETE clause introspected correctly';

            is $rel_info->{attrs}{on_update}, 'SET NULL',
                'ON UPDATE clause introspected correctly';

            is $rel_info->{attrs}{is_deferrable}, 1,
                'DEFERRABLE clause introspected correctly';

            foreach my $db_schema (['dbicsl-test', 'dbicsl.test'], '%') {
                lives_and {
                    rmtree EXTRA_DUMP_DIR;

                    my @warns;
                    local $SIG{__WARN__} = sub {
                        push @warns, $_[0] unless $_[0] =~ /\bcollides\b/;
                    };

                    make_schema_at(
                        'PGMultiSchema',
                        {
                            naming => 'current',
                            db_schema => $db_schema,
                            preserve_case => 1,
                            dump_directory => EXTRA_DUMP_DIR,
                            quiet => 1,
                        },
                        [ $dsn, $user, $password, {
                            on_connect_do  => [ 'SET client_min_messages=WARNING' ],
                        } ],
                    );

                    diag join "\n", @warns if @warns;

                    is @warns, 0;
                } 'dumped schema for "dbicsl-test" and "dbicsl.test" schemas with no warnings';

                my ($test_schema, $rsrc, $rs, $row, %uniqs, $rel_info);

                lives_and {
                    ok $test_schema = PGMultiSchema->connect($dsn, $user, $password, {
                        on_connect_do  => [ 'SET client_min_messages=WARNING' ],
                    });
                } 'connected test schema';

                lives_and {
                    ok $rsrc = $test_schema->source('PgLoaderTest4');
                } 'got source for table in schema name with dash';

                is try { $rsrc->column_info('id')->{is_auto_increment} }, 1,
                    'column in schema name with dash';

                is try { $rsrc->column_info('value')->{data_type} }, 'varchar',
                    'column in schema name with dash';

                is try { $rsrc->column_info('value')->{size} }, 100,
                    'column in schema name with dash';

                lives_and {
                    ok $rs = $test_schema->resultset('PgLoaderTest4');
                } 'got resultset for table in schema name with dash';

                lives_and {
                    ok $row = $rs->create({ value => 'foo' });
                } 'executed SQL on table in schema name with dash';

                $rel_info = try { $rsrc->relationship_info('dbicsl_dash_test_pg_loader_test5') };

                is_deeply $rel_info->{cond}, {
                    'foreign.four_id' => 'self.id'
                }, 'relationship in schema name with dash';

                is $rel_info->{attrs}{accessor}, 'single',
                    'relationship in schema name with dash';

                is $rel_info->{attrs}{join_type}, 'LEFT',
                    'relationship in schema name with dash';

                lives_and {
                    ok $rsrc = $test_schema->source('DbicslDashTestPgLoaderTest5');
                } 'got source for table in schema name with dash';

                %uniqs = try { $rsrc->unique_constraints };

                is keys %uniqs, 2,
                    'got unique and primary constraint in schema name with dash';

                delete $uniqs{primary};

                is_deeply ((values %uniqs)[0], ['four_id'],
                    'unique constraint is correct in schema name with dash');

                lives_and {
                    ok $rsrc = $test_schema->source('PgLoaderTest6');
                } 'got source for table in schema name with dot';

                is try { $rsrc->column_info('id')->{is_auto_increment} }, 1,
                    'column in schema name with dot introspected correctly';

                is try { $rsrc->column_info('value')->{data_type} }, 'varchar',
                    'column in schema name with dot introspected correctly';

                is try { $rsrc->column_info('value')->{size} }, 100,
                    'column in schema name with dot introspected correctly';

                lives_and {
                    ok $rs = $test_schema->resultset('PgLoaderTest6');
                } 'got resultset for table in schema name with dot';

                lives_and {
                    ok $row = $rs->create({ value => 'foo' });
                } 'executed SQL on table in schema name with dot';

                $rel_info = try { $rsrc->relationship_info('pg_loader_test7') };

                is_deeply $rel_info->{cond}, {
                    'foreign.six_id' => 'self.id'
                }, 'relationship in schema name with dot';

                is $rel_info->{attrs}{accessor}, 'single',
                    'relationship in schema name with dot';

                is $rel_info->{attrs}{join_type}, 'LEFT',
                    'relationship in schema name with dot';

                lives_and {
                    ok $rsrc = $test_schema->source('PgLoaderTest7');
                } 'got source for table in schema name with dot';

                %uniqs = try { $rsrc->unique_constraints };

                is keys %uniqs, 2,
                    'got unique and primary constraint in schema name with dot';

                delete $uniqs{primary};

                is_deeply ((values %uniqs)[0], ['six_id'],
                    'unique constraint is correct in schema name with dot');

                lives_and {
                    ok $test_schema->source('PgLoaderTest6')
                        ->has_relationship('pg_loader_test4');
                } 'cross-schema relationship in multi-db_schema';

                lives_and {
                    ok $test_schema->source('PgLoaderTest4')
                        ->has_relationship('pg_loader_test6s');
                } 'cross-schema relationship in multi-db_schema';

                lives_and {
                    ok $test_schema->source('PgLoaderTest8')
                        ->has_relationship('pg_loader_test7');
                } 'cross-schema relationship in multi-db_schema';

                lives_and {
                    ok $test_schema->source('PgLoaderTest7')
                        ->has_relationship('pg_loader_test8s');
                } 'cross-schema relationship in multi-db_schema';
            }
        },
    },
);

if( !$dsn || !$user ) {
    $tester->skip_tests('You need to set the DBICTEST_PG_DSN, _USER, and _PASS environment variables');
}
else {
    $tester->run_tests();
}

END {
    rmtree EXTRA_DUMP_DIR unless $ENV{SCHEMA_LOADER_TESTS_NOCLEANUP};
}
# vim:et sw=4 sts=4 tw=0:
