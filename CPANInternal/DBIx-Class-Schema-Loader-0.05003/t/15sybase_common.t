use strict;
use lib qw(t/lib);
use dbixcsl_common_tests;
use Test::More;
use Test::Exception;
use List::MoreUtils 'apply';

my $dsn      = $ENV{DBICTEST_SYBASE_DSN} || '';
my $user     = $ENV{DBICTEST_SYBASE_USER} || '';
my $password = $ENV{DBICTEST_SYBASE_PASS} || '';

my $tester = dbixcsl_common_tests->new(
    vendor      => 'sybase',
    auto_inc_pk => 'INTEGER IDENTITY NOT NULL PRIMARY KEY',
    default_function     => 'getdate()',
    default_function_def => 'AS getdate()',
    dsn         => $dsn,
    user        => $user,
    password    => $password,
    extra       => {
        create  => [
            q{
                CREATE TABLE sybase_loader_test1 (
                    id INTEGER IDENTITY NOT NULL PRIMARY KEY,
                    ts timestamp,
                    computed_dt AS getdate()
                )
            },
# Test data types, see http://ispirer.com/wiki/sqlways/sybase/data-types
# XXX handle FLOAT(P) at some point
# ( http://msdn.microsoft.com/en-us/library/aa258876(SQL.80).aspx )
            q{
                CREATE TABLE sybase_loader_test2 (
                    id INTEGER IDENTITY NOT NULL PRIMARY KEY,
                    a_text TEXT,
                    a_unitext UNITEXT,
                    an_image IMAGE,
                    a_bigint BIGINT,
                    an_int INT,
                    an_integer INTEGER,
                    a_smallint SMALLINT,
                    a_tinyint TINYINT,
                    a_real REAL,
                    a_double_precision DOUBLE PRECISION,
                    a_date DATE,
                    a_time TIME,
                    a_datetime DATETIME,
                    a_smalldatetime SMALLDATETIME,
                    a_money MONEY,
                    a_smallmoney SMALLMONEY,
                    a_timestamp timestamp,
                    a_bit BIT,
                    a_char_with_precision CHAR(2),
                    an_nchar_with_precision NCHAR(2),
                    a_unichar_with_precision UNICHAR(2),
                    a_varchar_with_precision VARCHAR(2),
                    an_nvarchar_with_precision NVARCHAR(2),
                    a_univarchar_with_precision UNIVARCHAR(2),
                    a_float FLOAT,
                    a_binary_with_precision BINARY(2),
                    a_varbinary_with_precision VARBINARY(2),
                    the_numeric NUMERIC(6,3),
                    the_decimal DECIMAL(6,3)
                )
            },
        ],
        drop  => [ qw/ sybase_loader_test1 sybase_loader_test2 / ],
        count => 36,
        run   => sub {
            my ($schema, $monikers, $classes) = @_;

            my $rs = $schema->resultset($monikers->{sybase_loader_test1});
            my $rsrc = $rs->result_source;

            is $rsrc->column_info('id')->{data_type},
                'int',
                'INTEGER IDENTITY data_type is correct';

            is $rsrc->column_info('id')->{is_auto_increment},
                1,
                'INTEGER IDENTITY is_auto_increment => 1';

            is $rsrc->column_info('ts')->{data_type},
               'timestamp',
               'timestamps have the correct data_type';

            is $rsrc->column_info('ts')->{inflate_datetime},
                0,
                'timestamps have inflate_datetime => 0';

            ok ((exists $rsrc->column_info('computed_dt')->{data_type}
              && (not defined $rsrc->column_info('computed_dt')->{data_type})),
                'data_type for computed column exists and is undef')
            or diag "Data type is: ",
                $rsrc->column_info('computed_dt')->{data_type}
            ;

            my $computed_dt_default =
                $rsrc->column_info('computed_dt')->{default_value};

            ok ((ref $computed_dt_default eq 'SCALAR'),
                'default_value for computed column is a scalar ref')
            or diag "default_value is: ", $computed_dt_default
            ;

            eval { is $$computed_dt_default,
                'getdate()',
                'default_value for computed column is correct' };

            $rsrc = $schema->resultset($monikers->{sybase_loader_test2})
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
                $data_type =~ s/_/ /g;

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
    $tester->skip_tests('You need to set the DBICTEST_SYBASE_DSN, _USER, and _PASS environment variables');
}
else {
    $tester->run_tests();
}
