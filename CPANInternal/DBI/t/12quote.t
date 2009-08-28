#!perl -w

use lib qw(blib/arch blib/lib);	# needed since -T ignores PERL5LIB
use strict;

use Test::More tests => 10;

use DBI qw(:sql_types);
use Config;
use Cwd;

$^W = 1;
$| = 1;

my $dbh = DBI->connect('dbi:ExampleP:', '', '');

sub check_quote {
	# checking quote
	is($dbh->quote("quote's"),         "'quote''s'", '... quoting strings with embedded single quotes');
	is($dbh->quote("42", SQL_VARCHAR), "'42'",       '... quoting number as SQL_VARCHAR');
	is($dbh->quote("42", SQL_INTEGER), "42",         '... quoting number as SQL_INTEGER');
	is($dbh->quote(undef),		   "NULL",	 '... quoting undef as NULL');
}

check_quote();

sub check_quote_identifier {

	is($dbh->quote_identifier('foo'),             '"foo"',       '... properly quotes foo as "foo"');
	is($dbh->quote_identifier('f"o'),             '"f""o"',      '... properly quotes f"o as "f""o"');
	is($dbh->quote_identifier('foo','bar'),       '"foo"."bar"', '... properly quotes foo, bar as "foo"."bar"');
	is($dbh->quote_identifier(undef,undef,'bar'), '"bar"',       '... properly quotes undef, undef, bar as "bar"');

	is($dbh->quote_identifier('foo',undef,'bar'), '"foo"."bar"', '... properly quotes foo, undef, bar as "foo"."bar"');

    SKIP: {
        skip "Can't test alternate quote_identifier logic with DBI_AUTOPROXY", 1
            if $ENV{DBI_AUTOPROXY};
        my $qi = $dbh->{dbi_quote_identifier_cache} || die "test out of date with dbi internals?";
	$qi->[1] = '@';   # SQL_CATALOG_NAME_SEPARATOR
	$qi->[2] = 2;     # SQL_CATALOG_LOCATION
	is($dbh->quote_identifier('foo',undef,'bar'), '"bar"@"foo"', '... now quotes it as "bar"@"foo" after flushing cache');
    }
}

check_quote_identifier();

1;
