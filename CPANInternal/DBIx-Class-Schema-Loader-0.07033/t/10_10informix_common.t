use strict;
use warnings;
use Test::More;
use Test::Exception;
use Try::Tiny;
use File::Path 'rmtree';
use DBIx::Class::Schema::Loader 'make_schema_at';
use DBIx::Class::Schema::Loader::Utils 'split_name';
use String::ToIdentifier::EN::Unicode 'to_identifier';
use namespace::clean;

use lib qw(t/lib);

use dbixcsl_common_tests ();
use dbixcsl_test_dir '$tdir';

use constant EXTRA_DUMP_DIR => "$tdir/informix_extra_dump";

# to support " quoted identifiers
BEGIN { $ENV{DELIMIDENT} = 'y' }

# This test doesn't run over a shared memory connection, because of the single connection limit.

my $dsn      = $ENV{DBICTEST_INFORMIX_DSN} || '';
my $user     = $ENV{DBICTEST_INFORMIX_USER} || '';
my $password = $ENV{DBICTEST_INFORMIX_PASS} || '';

my ($schema, $extra_schema); # for cleanup in END for extra tests

my $tester = dbixcsl_common_tests->new(
    vendor         => 'Informix',
    auto_inc_pk    => 'serial primary key',
    null           => '',
    default_function     => 'current year to fraction(5)',
    default_function_def => 'datetime year to fraction(5) default current year to fraction(5)',
    dsn            => $dsn,
    user           => $user,
    password       => $password,
    loader_options => { preserve_case => 1 },
    quote_char     => '"',
    data_types => {
        # http://publib.boulder.ibm.com/infocenter/idshelp/v115/index.jsp?topic=/com.ibm.sqlr.doc/ids_sqr_094.htm

        # Numeric Types
        'int'              => { data_type => 'integer' },
        integer            => { data_type => 'integer' },
        int8               => { data_type => 'bigint' },
        bigint             => { data_type => 'bigint' },
        serial             => { data_type => 'integer', is_auto_increment => 1 },
        bigserial          => { data_type => 'bigint',  is_auto_increment => 1 },
        serial8            => { data_type => 'bigint',  is_auto_increment => 1 },
        smallint           => { data_type => 'smallint' },
        real               => { data_type => 'real' },
        smallfloat         => { data_type => 'real' },
        # just 'double' is a syntax error
        'double precision' => { data_type => 'double precision' },
        float              => { data_type => 'double precision' },
        'float(1)'         => { data_type => 'double precision' },
        'float(5)'         => { data_type => 'double precision' },
        'float(10)'        => { data_type => 'double precision' },
        'float(15)'        => { data_type => 'double precision' },
        'float(16)'        => { data_type => 'double precision' },
        numeric            => { data_type => 'numeric' },
        decimal            => { data_type => 'numeric' },
        dec                => { data_type => 'numeric' },
	'numeric(6,3)'     => { data_type => 'numeric', size => [6,3] },
	'decimal(6,3)'     => { data_type => 'numeric', size => [6,3] },
	'dec(6,3)'         => { data_type => 'numeric', size => [6,3] },

        # Boolean Type
        # XXX this should map to 'boolean'
        boolean            => { data_type => 'smallint' },

        # Money Type
        money              => { data_type => 'money' },
        'money(3,3)'       => { data_type => 'numeric', size => [3,3] },

        # Byte Type
        byte               => { data_type => 'bytea', original => { data_type => 'byte' } },

        # Character String Types
        char               => { data_type => 'char', size => 1 },
        'char(3)'          => { data_type => 'char', size => 3 },
        character          => { data_type => 'char', size => 1 },
        'character(3)'     => { data_type => 'char', size => 3 },
        'varchar(3)'       => { data_type => 'varchar', size => 3 },
        'character varying(3)'
                           => { data_type => 'varchar', size => 3 },
        # XXX min size not supported, colmin from syscolumns is NULL
        'varchar(3,2)'     => { data_type => 'varchar', size => 3 },
        'character varying(3,2)'
                           => { data_type => 'varchar', size => 3 },
        nchar              => { data_type => 'nchar', size => 1 },
        'nchar(3)'         => { data_type => 'nchar', size => 3 },
        'nvarchar(3)'      => { data_type => 'nvarchar', size => 3 },
        'nvarchar(3,2)'    => { data_type => 'nvarchar', size => 3 },
        'lvarchar(3)'      => { data_type => 'lvarchar', size => 3 },
        'lvarchar(33)'     => { data_type => 'lvarchar', size => 33 },
        text               => { data_type => 'text' },

        # DateTime Types
        date               => { data_type => 'date' },
        'date default today'
                           => { data_type => 'date', default_value => \'today' },
        # XXX support all precisions
        'datetime year to fraction(5)',
                           => { data_type => 'datetime year to fraction(5)' },
        'datetime year to fraction(5) default current year to fraction(5)',
                           => { data_type => 'datetime year to fraction(5)', default_value => \'current year to fraction(5)' },
        # XXX do interval

        # Blob Types
        # XXX no way to distinguish opaque types boolean, blob and clob
        blob               => { data_type => 'blob' },
        clob               => { data_type => 'blob' },

        # IDSSECURITYLABEL Type
        #
        # This requires the DBSECADM privilege and a security policy on the
        # table, things I know nothing about.
#        idssecuritylabel   => { data_type => 'idssecuritylabel' },

        # List Types
        # XXX need to introspect element type too
        'list(varchar(20) not null)'
                           => { data_type => 'list' },
        'multiset(varchar(20) not null)'
                           => { data_type => 'multiset' },
        'set(varchar(20) not null)'
                           => { data_type => 'set' },
    },
    extra => {
        count => 26 * 2,
        run   => sub {
            ($schema) = @_;

            SKIP: {
                skip 'Set the DBICTEST_INFORMIX_EXTRADB_DSN, _USER and _PASS environment variables to run the multi-database tests', 26 * 2
                    unless $ENV{DBICTEST_INFORMIX_EXTRADB_DSN};

                $extra_schema = $schema->clone;
                $extra_schema->connection(@ENV{map "DBICTEST_INFORMIX_EXTRADB_$_",
                    qw/DSN USER PASS/
                });

                my $dbh1 = $schema->storage->dbh;

                $dbh1->do(<<'EOF');
                    CREATE TABLE informix_loader_test4 (
                        id SERIAL PRIMARY KEY,
                        value VARCHAR(100)
                    )
EOF
                $dbh1->do(<<'EOF');
                    CREATE TABLE informix_loader_test5 (
                        id SERIAL PRIMARY KEY,
                        value VARCHAR(100),
                        four_id INTEGER REFERENCES informix_loader_test4 (id)
                    )
EOF
                $dbh1->do(<<'EOF');
ALTER TABLE informix_loader_test5 ADD CONSTRAINT UNIQUE (four_id) CONSTRAINT loader_test5_uniq
EOF

                my $db1 = db_name($schema);

                $dbh1->disconnect;

                my $dbh2 = $extra_schema->storage->dbh;

                $dbh2->do(<<'EOF');
                    CREATE TABLE informix_loader_test5 (
                        pk SERIAL PRIMARY KEY,
                        value VARCHAR(100),
                        four_id INTEGER
                    )
EOF
                $dbh2->do(<<'EOF');
ALTER TABLE informix_loader_test5 ADD CONSTRAINT UNIQUE (four_id) CONSTRAINT loader_test5_uniq
EOF
                $dbh2->do(<<"EOF");
                    CREATE TABLE informix_loader_test6 (
                        id SERIAL PRIMARY KEY,
                        value VARCHAR(100)
                    )
EOF
                $dbh2->do(<<"EOF");
                    CREATE TABLE informix_loader_test7 (
                        id SERIAL PRIMARY KEY,
                        value VARCHAR(100),
                        six_id INTEGER UNIQUE REFERENCES informix_loader_test6 (id)
                    )
EOF

                my $db2 = db_name($extra_schema);

                $dbh2->disconnect;

                my $db1_moniker = join '', map ucfirst lc, split_name to_identifier $db1;
                my $db2_moniker = join '', map ucfirst lc, split_name to_identifier $db2;

                foreach my $db_schema ({ $db1 => '%', $db2 => '%' }, { '%' => '%' }) {
                    lives_and {
                        my @warns;
                        local $SIG{__WARN__} = sub {
                            push @warns, $_[0] unless $_[0] =~ /\bcollides\b/
                                || $_[0] =~ /unreferencable/;
                        };
     
                        make_schema_at(
                            'InformixMultiDatabase',
                            {
                                naming => 'current',
                                db_schema => $db_schema,
                                dump_directory => EXTRA_DUMP_DIR,
                                quiet => 1,
                            },
                            [ $dsn, $user, $password ],
                        );

                        InformixMultiDatabase->storage->disconnect;

                        diag join "\n", @warns if @warns;

                        is @warns, 0;
                    } "dumped schema for databases $db1 and $db2 with no warnings";

                    my $test_schema;

                    lives_and {
                        ok $test_schema = InformixMultiDatabase->connect($dsn, $user, $password);
                    } 'connected test schema';

                    my ($rsrc, $rs, $row, $rel_info, %uniqs);

                    lives_and {
                        ok $rsrc = $test_schema->source("InformixLoaderTest4");
                    } 'got source for table in database one';

                    is try { $rsrc->column_info('id')->{is_auto_increment} }, 1,
                        'column in database one';

                    is try { $rsrc->column_info('value')->{data_type} }, 'varchar',
                        'column in database one';

                    is try { $rsrc->column_info('value')->{size} }, 100,
                        'column in database one';

                    lives_and {
                        ok $rs = $test_schema->resultset("InformixLoaderTest4");
                    } 'got resultset for table in database one';

                    lives_and {
                        ok $row = $rs->create({ value => 'foo' });
                    } 'executed SQL on table in database one';

                    $rel_info = try { $rsrc->relationship_info("informix_loader_test5") };

                    is_deeply $rel_info->{cond}, {
                        'foreign.four_id' => 'self.id'
                    }, 'relationship in database one';

                    is $rel_info->{attrs}{accessor}, 'single',
                        'relationship in database one';

                    is $rel_info->{attrs}{join_type}, 'LEFT',
                        'relationship in database one';

                    lives_and {
                        ok $rsrc = $test_schema->source("${db1_moniker}InformixLoaderTest5");
                    } 'got source for table in database one';

                    %uniqs = try { $rsrc->unique_constraints };

                    is keys %uniqs, 2,
                        'got unique and primary constraint in database one';

                    delete $uniqs{primary};

                    is_deeply ((values %uniqs)[0], ['four_id'],
                        'correct unique constraint in database one');

                    lives_and {
                        ok $rsrc = $test_schema->source("InformixLoaderTest6");
                    } 'got source for table in database two';

                    is try { $rsrc->column_info('id')->{is_auto_increment} }, 1,
                        'column in database two introspected correctly';

                    is try { $rsrc->column_info('value')->{data_type} }, 'varchar',
                        'column in database two introspected correctly';

                    is try { $rsrc->column_info('value')->{size} }, 100,
                        'column in database two introspected correctly';

                    lives_and {
                        ok $rs = $test_schema->resultset("InformixLoaderTest6");
                    } 'got resultset for table in database two';

                    lives_and {
                        ok $row = $rs->create({ value => 'foo' });
                    } 'executed SQL on table in database two';

                    $rel_info = try { $rsrc->relationship_info('informix_loader_test7') };

                    is_deeply $rel_info->{cond}, {
                        'foreign.six_id' => 'self.id'
                    }, 'relationship in database two';

                    is $rel_info->{attrs}{accessor}, 'single',
                        'relationship in database two';

                    is $rel_info->{attrs}{join_type}, 'LEFT',
                        'relationship in database two';

                    lives_and {
                        ok $rsrc = $test_schema->source("InformixLoaderTest7");
                    } 'got source for table in database two';

                    %uniqs = try { $rsrc->unique_constraints };

                    is keys %uniqs, 2,
                        'got unique and primary constraint in database two';

                    delete $uniqs{primary};

                    is_deeply ((values %uniqs)[0], ['six_id'],
                        'correct unique constraint in database two');
                }
            }
        },
    },
);

if( !$dsn ) {
    $tester->skip_tests('You need to set the DBICTEST_INFORMIX_DSN, _USER, and _PASS environment variables');
}
else {
    $tester->run_tests();
}

sub db_name {
    my $schema = shift;

    # When we clone the schema, it still references the original loader, which
    # references the original schema.
    local $schema->loader->{schema} = $schema;

    return $schema->loader->_current_db;

    $schema->storage->disconnect;
}

END {
    if (not $ENV{SCHEMA_LOADER_TESTS_NOCLEANUP}) {
        if (my $dbh2 = try { $extra_schema->storage->dbh }) {

            try {
                $dbh2->do('DROP TABLE informix_loader_test7');
                $dbh2->do('DROP TABLE informix_loader_test6');
                $dbh2->do('DROP TABLE informix_loader_test5');
            }
            catch {
                die "Error dropping test tables: $_";
            };

            $dbh2->disconnect;
        }

        if (my $dbh1 = try { $schema->storage->dbh }) {
            
            try {
                $dbh1->do('DROP TABLE informix_loader_test5');
                $dbh1->do('DROP TABLE informix_loader_test4');
            }
            catch {
                die "Error dropping test tables: $_";
            };

            $dbh1->disconnect;
        }

        rmtree EXTRA_DUMP_DIR;
    }
}
# vim:et sts=4 sw=4 tw=0:
