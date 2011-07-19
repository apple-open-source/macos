use strict;
use lib qw(t/lib);
use dbixcsl_common_tests;
use Test::More;
use List::MoreUtils 'apply';

my $dsn      = $ENV{DBICTEST_PG_DSN} || '';
my $user     = $ENV{DBICTEST_PG_USER} || '';
my $password = $ENV{DBICTEST_PG_PASS} || '';

my $tester = dbixcsl_common_tests->new(
    vendor      => 'Pg',
    auto_inc_pk => 'SERIAL NOT NULL PRIMARY KEY',
    default_function => 'now()',
    dsn         => $dsn,
    user        => $user,
    password    => $password,
    extra       => {
        create => [
            q{
                CREATE TABLE pg_loader_test1 (
                    id SERIAL NOT NULL PRIMARY KEY,
                    value VARCHAR(100)
                )
            },
            q{
                COMMENT ON TABLE pg_loader_test1 IS 'The Table'
            },
            q{
                COMMENT ON COLUMN pg_loader_test1.value IS 'The Column'
            },
            q{
                CREATE TABLE pg_loader_test2 (
                    id SERIAL NOT NULL PRIMARY KEY,
                    value VARCHAR(100)
                )
            },
            q{
                COMMENT ON TABLE pg_loader_test2 IS 'very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very long comment'
            },
            # Test to make sure data_types that don't need a size => don't
            # have one and check varying types have the correct size.
            q{
                CREATE TABLE pg_loader_test3 (
                    id SERIAL NOT NULL PRIMARY KEY,
                    a_bigint BIGINT,
                    an_int8 INT8,
                    a_bigserial BIGSERIAL,
                    a_serial8 SERIAL8,
                    a_bit BIT,
                    a_boolean BOOLEAN,
                    a_bool BOOL,
                    a_box BOX,
                    a_bytea BYTEA,
                    a_cidr CIDR,
                    a_circle CIRCLE,
                    a_date DATE,
                    a_double_precision DOUBLE PRECISION,
                    a_float8 FLOAT8,
                    an_inet INET,
                    an_integer INTEGER,
                    an_int INT,
                    an_int4 INT4,
                    an_interval INTERVAL,
                    an_interval_with_precision INTERVAL(2),
                    a_line LINE,
                    an_lseg LSEG,
                    a_macaddr MACADDR,
                    a_money MONEY,
                    a_path PATH,
                    a_point POINT,
                    a_polygon POLYGON,
                    a_real REAL,
                    a_float4 FLOAT4,
                    a_smallint SMALLINT,
                    an_int2 INT2,
                    a_serial SERIAL,
                    a_serial4 SERIAL4,
                    a_text TEXT,
                    a_time TIME,
                    a_time_with_precision TIME(2),
                    a_time_without_time_zone TIME WITHOUT TIME ZONE,
                    a_time_without_time_zone_with_precision TIME(2) WITHOUT TIME ZONE,
                    a_time_with_time_zone TIME WITH TIME ZONE,
                    a_time_with_time_zone_with_precision TIME(2) WITH TIME ZONE,
                    a_timestamp TIMESTAMP,
                    a_timestamp_with_precision TIMESTAMP(2),
                    a_timestamp_without_time_zone TIMESTAMP WITHOUT TIME ZONE,
                    a_timestamp_without_time_zone_with_precision TIMESTAMP(2) WITHOUT TIME ZONE,
                    a_timestamp_with_time_zone TIMESTAMP WITH TIME ZONE,
                    a_timestamp_with_time_zone_with_precision TIMESTAMP(2) WITH TIME ZONE,
                    a_bit_varying_with_precision BIT VARYING(2),
                    a_varbit_with_precision VARBIT(2),
                    a_character_varying_with_precision CHARACTER VARYING(2),
                    a_varchar_with_precision VARCHAR(2),
                    a_character_with_precision CHARACTER(2),
                    a_char_with_precision CHAR(2),
                    the_numeric NUMERIC(6, 3),
                    the_decimal DECIMAL(6, 3)
                )
            },
        ],
        drop  => [ qw/ pg_loader_test1 pg_loader_test2 pg_loader_test3 / ],
        count => 57,
        run   => sub {
            my ($schema, $monikers, $classes) = @_;

            my $class    = $classes->{pg_loader_test1};
            my $filename = $schema->_loader->_get_dump_filename($class);

            my $code = do {
                local ($/, @ARGV) = (undef, $filename);
                <>;
            };

            like $code, qr/^=head1 NAME\n\n^$class - The Table\n\n^=cut\n/m,
                'table comment';

            like $code, qr/^=head2 value\n\n(.+:.+\n)+\nThe Column\n\n/m,
                'column comment and attrs';

            $class    = $classes->{pg_loader_test2};
            $filename = $schema->_loader->_get_dump_filename($class);

            $code = do {
                local ($/, @ARGV) = (undef, $filename);
                <>;
            };

            like $code, qr/^=head1 NAME\n\n^$class\n\n=head1 DESCRIPTION\n\n^very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very long comment\n\n^=cut\n/m,
                'long table comment is in DESCRIPTION';

            my $rsrc = $schema->resultset($monikers->{pg_loader_test3})
                ->result_source;
            my @type_columns = grep /^a/, $rsrc->columns;
            my @without_precision = grep !/_with_precision\z/, @type_columns;
            my @with_precision    = grep  /_with_precision\z/, @type_columns;
            my %with_precision;
            @with_precision{
                apply { s/_with_precision\z// } @with_precision
            } = ();

            for my $col (@without_precision) {
                my ($data_type) = $col =~ /^an?_(.*)/;
                ($data_type = uc $data_type) =~ s/_/ /g;

                ok((not exists $rsrc->column_info($col)->{size}),
                    "$data_type " .
                    (exists $with_precision{$col} ? 'without precision ' : '') .
                    "has no 'size' column_info")
                or diag "size is ".$rsrc->column_info($col)->{size}."\n";
            }

            for my $col (@with_precision) {
                my ($data_type) = $col =~ /^an?_(.*)_with_precision\z/;
                ($data_type = uc $data_type) =~ s/_/ /g;
                my $size = $rsrc->column_info($col)->{size};

                is $size, 2,
                  "$data_type with precision has a correct 'size' column_info";
            }

            is_deeply $rsrc->column_info('the_numeric')->{size}, [6,3],
                'size for NUMERIC(precision, scale) is correct';

            is_deeply $rsrc->column_info('the_decimal')->{size}, [6,3],
                'size for DECIMAL(precision, scale) is correct';
        },
    },
);

if( !$dsn || !$user ) {
    $tester->skip_tests('You need to set the DBICTEST_PG_DSN, _USER, and _PASS environment variables');
}
else {
    $tester->run_tests();
}
