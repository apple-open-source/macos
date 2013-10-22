use strict;
use warnings;
use Test::More;
use DBIx::Class::Schema::Loader::DBI::ODBC::ACCESS ();
use lib qw(t/lib);
use dbixcsl_common_tests;

my $odbc_dsn      = $ENV{DBICTEST_MSACCESS_ODBC_DSN} || '';
my $odbc_user     = $ENV{DBICTEST_MSACCESS_ODBC_USER} || '';
my $odbc_password = $ENV{DBICTEST_MSACCESS_ODBC_PASS} || '';

my $ado_dsn       = $ENV{DBICTEST_MSACCESS_ADO_DSN} || '';
my $ado_user      = $ENV{DBICTEST_MSACCESS_ADO_USER} || '';
my $ado_password  = $ENV{DBICTEST_MSACCESS_ADO_PASS} || '';

my %ado_extra_types = (
    'tinyint'     => { data_type => 'tinyint', original => { data_type => 'byte' } },
    'smallmoney'  => { data_type => 'money', original => { data_type => 'currency' } },
    'decimal'     => { data_type => 'decimal' },
    'decimal(3)'  => { data_type => 'decimal', size => [3, 0] },
    'decimal(3,3)'=> { data_type => 'decimal', size => [3, 3] },
    'dec(5,5)'    => { data_type => 'decimal', size => [5, 5] },
    'numeric(2,2)'=> { data_type => 'decimal', size => [2, 2] },
    'character'   => { data_type => 'char', size => 255 },
    'character varying(5)'  => { data_type => 'varchar', size => 5 },
    'nchar(5)'    => { data_type => 'char', size => 5 },
    'national character(5)' => { data_type => 'char', size => 5 },
    'nvarchar(5)' => { data_type => 'varchar', size => 5 },
    'national character varying(5)' => { data_type => 'varchar', size => 5 },
    'national char varying(5)' => { data_type => 'varchar', size => 5 },
    'smalldatetime' => { data_type => 'datetime' },
    'uniqueidentifier' => { data_type => 'uniqueidentifier', original => { data_type => 'guid' } },
    'text'        => { data_type => 'text', original => { data_type => 'longchar' } },
    'ntext'       => { data_type => 'text', original => { data_type => 'longchar' } },
);

my $tester = dbixcsl_common_tests->new(
    vendor      => 'Access',
    auto_inc_pk => 'AUTOINCREMENT PRIMARY KEY',
    quote_char  => [qw/[ ]/],
    connect_info => [ ($odbc_dsn ? {
            dsn         => $odbc_dsn,
            user        => $odbc_user,
            password    => $odbc_password,
        } : ()),
        ($ado_dsn ? {
            dsn         => $ado_dsn,
            user        => $ado_user,
            password    => $ado_password,
        } : ()),
    ],
    data_types  => {
        # http://msdn.microsoft.com/en-us/library/bb208866(v=office.12).aspx
        #
        # Numeric types
        'autoincrement'=>{ data_type => 'integer', is_auto_increment => 1 },
        'int'         => { data_type => 'integer' },
        'integer'     => { data_type => 'integer' },
        'long'        => { data_type => 'integer' },
        'integer4'    => { data_type => 'integer' },
        'smallint'    => { data_type => 'smallint' },
        'short'       => { data_type => 'smallint' },
        'integer2'    => { data_type => 'smallint' },
        'integer1'    => { data_type => 'tinyint', original => { data_type => 'byte' } },
        'byte'        => { data_type => 'tinyint', original => { data_type => 'byte' } },
        'bit'         => { data_type => 'bit' },
        'logical'     => { data_type => 'bit' },
        'logical1'    => { data_type => 'bit' },
        'yesno'       => { data_type => 'bit' },
        'money'       => { data_type => 'money', original => { data_type => 'currency' } },
        'currency'    => { data_type => 'money', original => { data_type => 'currency' } },
        'real'        => { data_type => 'real' },
        'single'      => { data_type => 'real' },
        'ieeesingle'  => { data_type => 'real' },
        'float4'      => { data_type => 'real' },
        'float'       => { data_type => 'double precision', original => { data_type => 'double' } },
        'float'       => { data_type => 'double precision', original => { data_type => 'double' } },
        'float8'      => { data_type => 'double precision', original => { data_type => 'double' } },
        'double'      => { data_type => 'double precision', original => { data_type => 'double' } },
        'ieeedouble'  => { data_type => 'double precision', original => { data_type => 'double' } },
        'number'      => { data_type => 'double precision', original => { data_type => 'double' } },

#        # character types
        'text(25)'    => { data_type => 'varchar', size => 25 },
        'char'        => { data_type => 'char', size => 255 },
        'char(5)'     => { data_type => 'char', size => 5 },
        'string(5)'   => { data_type => 'varchar', size => 5 },
        'varchar(5)'  => { data_type => 'varchar', size => 5 },

        # binary types
        'binary(10)'  => { data_type => 'binary', size => 10 },
        'varbinary(11)' => { data_type => 'varbinary', size => 11 },

        # datetime types
        'datetime'    => { data_type => 'datetime' },
        'time'        => { data_type => 'datetime' },
        'timestamp'   => { data_type => 'datetime' },

        # misc types
        'guid'        => { data_type => 'uniqueidentifier', original => { data_type => 'guid' } },

        # blob types
        'longchar'    => { data_type => 'text', original => { data_type => 'longchar' } },
        'longtext'    => { data_type => 'text', original => { data_type => 'longchar' } },
        'memo'        => { data_type => 'text', original => { data_type => 'longchar' } },
        'image'       => { data_type => 'image', original => { data_type => 'longbinary' } },
        'longbinary'  => { data_type => 'image', original => { data_type => 'longbinary' } },

        %ado_extra_types,
    },
    data_types_ddl_cb => sub {
        my $ddl = shift;
        {
            package DBIXCSL_Test::DummySchema;
            use base 'DBIx::Class::Schema';
        }
        my @connect_info = $odbc_dsn ? ($odbc_dsn, $odbc_user, $odbc_password)
                                     : ($ado_dsn,  $ado_user,  $ado_password);

        my $schema = DBIXCSL_Test::DummySchema->connect(@connect_info);

        my $loader = DBIx::Class::Schema::Loader::DBI::ODBC::ACCESS->new(
            schema => $schema,
            naming => 'current',
        );

        my $conn = $loader->_ado_connection;

        require Win32::OLE;
        my $comm = Win32::OLE->new('ADODB.Command');

        $comm->{ActiveConnection} = $conn;
        $comm->{CommandText}      = $ddl;
        $comm->Execute;
    },
);

if (not ($odbc_dsn || $ado_dsn)) {
    $tester->skip_tests('You need to set the DBICTEST_MSACCESS_ODBC_DSN, and optionally _USER and _PASS and/or the DBICTEST_MSACCESS_ADO_DSN, and optionally _USER and _PASS environment variables');
}
else {
    $tester->run_tests();
}

# vim:et sts=4 sw=4 tw=0:
