use strict;

use Test::More tests => 51;

use lib 't';
use Run;

require_ok('Exporter::Easy');

package Start::Testing::Use::Functions;

{
	no strict 'refs';

	*{add_tags} = \&Exporter::Easy::add_tags;
	*{expand_tags} = \&Exporter::Easy::expand_tags;
}

::ok(
	::eq_set( [ expand_tags([qw( a b c)], {}) ], [ qw( a b c) ] ),
	"simple _expand_tags"
);

::ok(
	::eq_set( [ expand_tags([qw( a b c !b)], {}) ], [ qw( a c ) ] ),
	"simple _expand_tags with remove"
);

::ok(
	::eq_set(
		[
			expand_tags([ qw( a b c :tag2 ) ], { tag2 => [ qw( d e ) ] }),
		],
		[qw( a b c d e )],
	),
	"_expand_tags with tag"
);

::ok(
	::eq_set(
		[
			expand_tags( [ qw( a b c d f !:tag2 ) ],{ tag2 => [ qw( d e ) ] })
		],
		[qw( a b c f )]
	),
	"_expand_tags with remove tag"
);

my $tags = add_tags(
	[
		tag1 => [qw( a b c d )],
		tag2 => [qw( c d e )],
		tag3 => [qw( :tag1 !:tag2 d !a )],
	]
);

::ok(::eq_set( $tags->{tag1}, [qw( a b c d )] ), "_build_all_tags tag1");
::ok(::eq_set( $tags->{tag2}, [qw( c d e )] ), "_build_all_tags tag2");
::ok(::eq_set( $tags->{tag3}, [qw( b d )] ), "_build_all_tags tag3");
::ok(keys(%$tags) == 3, "use TAGS count");

package Use::OK;

use Exporter::Easy (
	OK => [qw( o_1 o_2) ],
);

use vars qw( @EXPORT_OK );

::ok(::eq_set(\@EXPORT_OK, [qw( o_1 o_2 )]), "simple use OK");

package Use::OK_ONLY;

use Exporter::Easy (
	OK_ONLY => [qw( o_1 o_2 ) ],
);

use vars qw( @EXPORT_OK );

::ok(::eq_set(\@EXPORT_OK, [qw( o_1 o_2 )]), "simple use OK_ONLY");

package Use::More;

use Exporter::Easy (
	EXPORT => [ qw( e_1 e_2 ) ],
	FAIL => [qw( f_1 f_2) ],
	OK_ONLY => [qw( o_1 o_2) ],
);

use vars qw( @EXPORT @EXPORT_FAIL @EXPORT_OK %EXPORT_TAGS );

::ok(::eq_set( \@EXPORT, [qw( e_1 e_2)] ), "use EXPORT");
::ok(::eq_set( \@EXPORT_FAIL, [qw( f_1 f_2)] ), "use FAIL");
::ok(::eq_set( \@EXPORT_OK, [qw( o_1 o_2 )] ), "use OK_ONLY with EXPORT");
::is_deeply(\%EXPORT_TAGS, {}, "use without TAGS");

package Use::TAGS::And::OK_ONLY;

use Exporter::Easy (
	EXPORT => [ qw( e_1 e_2 ) ],
	TAGS => [
		tag1 => [qw( a b c d e f )],
		tag2 => [qw( b d f )],
		tag3 => [qw( :tag1 !:tag2 )],
	],
	OK_ONLY => [qw( o_1 o_2) ],
);

use vars qw( @EXPORT @EXPORT_OK %EXPORT_TAGS );

::ok(::eq_set( \@EXPORT, [ qw( e_1 e_2)] ), "use EXPORT and TAGS");
::ok(::eq_set( \@EXPORT_OK ,[qw( o_1 o_2 )] ), "use OK_ONLY with EXPORT and TAGS"
);

{
	my %e = %EXPORT_TAGS;

	::ok(::eq_set( $e{tag1}, [qw( a b c d e f )] ), "use TAGS tag1");
	::ok(::eq_set( $e{tag2}, [qw( b d f )] ), "use TAGS tag2");
	::ok(::eq_set( $e{tag3}, [qw( a c e )] ), "use TAGS tag3");
	::ok(keys(%e) == 3, "use TAGS count");
}

package Test::The::Use3;

use Exporter::Easy (
	EXPORT => [ qw( e_1 e_2 ) ],
	TAGS => [
		tag1 => [qw( a b c d e f )],
		tag2 => [qw( b d f )],
		tag3 => [qw( :tag1 !:tag2 )],
	],
	OK => [qw( o_1 o_2) ],
);

use vars qw( @EXPORT @EXPORT_OK %EXPORT_TAGS );

::ok(::eq_set( \@EXPORT, [ qw( e_1 e_2)] ), "use EXPORT and TAGS");
::ok(::eq_set( \@EXPORT_OK ,[qw( a b c d e f o_1 o_2 )] ), "use OK with EXPORT and TAGS"
);

{
	my %e = %EXPORT_TAGS;

	::ok(::eq_set( $e{tag1}, [qw( a b c d e f )] ), "use TAGS tag1");
	::ok(::eq_set( $e{tag2}, [qw( b d f )] ), "use TAGS tag2");
	::ok(::eq_set( $e{tag3}, [qw( a c e )] ), "use TAGS tag3");
	::ok(keys(%e) == 3, "use TAGS count");
}

package Test::The::Use4;

use Exporter::Easy (
	EXPORT => [qw( open close :rw )],
	FAIL => [qw( hello :fail )],
	TAGS => [
		fail => [qw (f_1 f_2 )],
		rw => [qw( read write )],
		sys => [qw( sysopen sysclose )],
	],
	ALL => 'all',
);

use vars qw( @EXPORT @EXPORT_OK @EXPORT_FAIL %EXPORT_TAGS );

::ok(::eq_set( \@EXPORT, [qw( open close read write)] ), "use tags in EXPORT");
::ok(::eq_set( \@EXPORT_OK, [qw( hello f_1 f_2 sysopen sysclose read write )]) , "use FAIL in EXPORT_OK");
::ok(::eq_set( \@EXPORT_FAIL, [qw( hello f_1 f_2 )] ), "use tags in EXPORT");
::ok(::eq_set( $EXPORT_TAGS{all}, [qw( hello f_1 f_2 read write sysopen sysclose open close )] ), "use ALL with FAIL");

package Test::The::Use5;

eval <<EOM;
use Exporter::Easy (
	EXPORT => [qw( :tag )],
);
EOM

::ok($@, "die for unknown tag");

package Test::ISA::Default;

use base 'base';
use vars '@ISA';

use Exporter::Easy(ALL => 'all');

::is_deeply(\@ISA, ['base','Exporter'], '@ISA default');

package Test::ISA::Explicit;

use base 'base';
use vars '@ISA';

use Exporter::Easy(
	ISA => 1,
);

::is_deeply(\@ISA, ['base','Exporter'], '@ISA explicit');

package Test::ISA::No;

use base 'base';
use vars '@ISA';

use Exporter::Easy(
	ISA => 0,
);

::is_deeply(\@ISA, ['base'], 'no @ISA explicit');

package Test::Vars;

use Exporter::Easy(
	TAGS => [
		var => [qw( $hello @hello %hello a )],
		not => [qw( $goodbye @goodbye %goodbye b )],
	],
);

foreach my $type (qw( $ @ % ))
{
	::runs_ok("${type}\{hello}", "tag vars can use var ${type}hello");

	::runs_ok("${type}\{goodbye}", "tag vars can't use var ${type}goodbye");
}

package Test::Vars::List;

use Exporter::Easy(
	TAGS => [
		var => [qw( $hello @hello %hello a )],
		not => [qw( $goodbye @goodbye %goodbye a )],
	],
	VARS => [':var', '$cat'],
);

foreach my $type (qw( $ @ % ))
{
	::runs_ok("${type}\{hello}", "list vars can use var ${type}hello");

	::dies_ok("${type}\{goodbye}", "list vars can't use var ${type}goodbye");
}

::runs_ok('$cat', 'list vars can use var $cat');

package Test::Vars::Fail;

use Exporter::Easy(
	TAGS => [
		not => [qw( $goodbye @goodbye %goodbye )],
	],
	VARS => 0,
);

foreach my $type (qw( $ @ % ))
{
	::dies_ok("${type}\{goodbye}", "no vars can't use var ${type}goodbye");
}
