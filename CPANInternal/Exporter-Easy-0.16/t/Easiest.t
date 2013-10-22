use strict;

use Test::More tests => 21;

use lib 't';
use Run;

require_ok('Exporter::Easiest');

{
	no strict 'refs';

	*{suck_list} = \&Exporter::Easiest::suck_list;
	*{parse_spec} = \&Exporter::Easiest::parse_spec;
}

is_deeply(suck_list([qw(a b c d e)]), [qw( a b c d e )], "suck all");
is_deeply(suck_list([qw(a b c => e)]), [qw( a b )], "suck some");

is_deeply(
	{
		parse_spec(q(
			a => a b c
		))
	},
	{
		a => [qw( a b c )],
	},
	"parse 1"
);

is_deeply(
	{
		parse_spec(q(
			a => a b c
			b => g h i
		))
	},
	{
		a => [qw( a b c )],
		b => [qw( g h i )],
	},
	"parse 2"
);

is_deeply(
	{
		parse_spec(q(
			a =>
			b => g h i
		))
	},
	{
		a => [],
		b => [qw( g h i )],
	},
	"parse with empty"
);

is_deeply(
	{
		parse_spec(q(
			a =>
				:b => a b :c
				:e => e f g
		))
	},
	{
		a => [],
		TAGS =>
		[
			'b', [qw( a b :c )],
			'e', [qw( e f g )],
		]
	},
	"simple with :s"
);

is_deeply(
	{
		parse_spec(q(
			b => a b
			a =>
				:b =>
				:e => e f :g
				:d => a
			c => a :c
		))
	},
	{
		a => [],
		TAGS =>
		[
			'b', [],
			'e', [qw( e f :g )],
			'd' => ['a'],
		],
		b => [qw( a b )],
		c => [qw( a :c)],
	},
	"everything"
);

is_deeply(
	{ parse_spec(q(VARS => a b)) },
	{ VARS => [qw( a b )] },
	"VARS list"
);

is_deeply(
	{ parse_spec(q(VARS => a)) },
	{ VARS => [qw( a )] },
	"VARS list of 1"
);
is_deeply(
	{ parse_spec(q(VARS => 1)) },
	{ VARS => 1 },
	"VARS 1"
);

is_deeply(
	{ parse_spec(q(VARS => 0)) },
	{ VARS => 0 },
	"VARS 0"
);

is_deeply(
	{ parse_spec(q(ALL => all)) },
	{ ALL => 'all' },
	"good ALL works"
);

eval {parse_spec(q(ALL => all other))};
ok($@, "bad all dies");

package Test::The::Use;

use Exporter::Easiest q(
	EXPORT => e_1 e_2
	TAGS =>
		:tag1 =>  a b c d e f
		:tag2 => b d f
		:tag3 => :tag1 !:tag2
	OK => o_1 o_2
);

use vars qw( @EXPORT @EXPORT_OK %EXPORT_TAGS );

::ok(::eq_set( \@EXPORT, [ qw( e_1 e_2)] ), "use EXPORT and TAGS");
::ok(::eq_set( \@EXPORT_OK ,[qw( a b c d e f o_1 o_2 )] ), "use OK with EXPORT and TAGS"
);

my %e = %EXPORT_TAGS;

::ok(::eq_set( $e{tag1}, [qw( a b c d e f )] ), "use TAGS tag1");
::ok(::eq_set( $e{tag2}, [qw( b d f )] ), "use TAGS tag2");
::ok(::eq_set( $e{tag3}, [qw( a c e )] ), "use TAGS tag3");
::ok(keys(%e) == 3, "use TAGS count");

package Test::Vars;

use Exporter::Easiest qw( OK => $Var );

::runs_ok('$Var', 'tag vars can use var $Var');
