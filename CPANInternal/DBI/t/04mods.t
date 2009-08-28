#!perl -w
$|=1;

use strict;

use Test::More tests => 12;

## ----------------------------------------------------------------------------
## 04mods.t - ...
## ----------------------------------------------------------------------------
# Note: 
# the modules tested here are all marked as new and not guaranteed, so this if
# they change, these will fail.
## ----------------------------------------------------------------------------

BEGIN { 
	use_ok( 'DBI' );
    
    # load these first, since the other two load them
    # and we want to catch the error first
    use_ok( 'DBI::Const::GetInfo::ANSI' );
    use_ok( 'DBI::Const::GetInfo::ODBC' );    
    
	use_ok( 'DBI::Const::GetInfoType',    qw(%GetInfoType) );
	use_ok( 'DBI::Const::GetInfoReturn',  qw(%GetInfoReturnTypes %GetInfoReturnValues) );
}

## test GetInfoType

cmp_ok(scalar(keys(%GetInfoType)), '>', 1, '... we have at least one key in the GetInfoType hash');

is_deeply(
    \%GetInfoType,
    { %DBI::Const::GetInfo::ANSI::InfoTypes, %DBI::Const::GetInfo::ODBC::InfoTypes },
    '... the GetInfoType hash is constructed from the ANSI and ODBC hashes'
    );

## test GetInfoReturnTypes

cmp_ok(scalar(keys(%GetInfoReturnTypes)), '>', 1, '... we have at least one key in the GetInfoReturnType hash');

is_deeply(
    \%GetInfoReturnTypes,
    { %DBI::Const::GetInfo::ANSI::ReturnTypes, %DBI::Const::GetInfo::ODBC::ReturnTypes },
    '... the GetInfoReturnType hash is constructed from the ANSI and ODBC hashes'
    );

## test GetInfoReturnValues

cmp_ok(scalar(keys(%GetInfoReturnValues)), '>', 1, '... we have at least one key in the GetInfoReturnValues hash');

# ... testing GetInfoReturnValues any further would be difficult

## test the two methods found in DBI::Const::GetInfoReturn

can_ok('DBI::Const::GetInfoReturn', 'Format');
can_ok('DBI::Const::GetInfoReturn', 'Explain');

1;
