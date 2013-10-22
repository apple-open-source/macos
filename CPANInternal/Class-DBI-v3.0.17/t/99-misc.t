use strict;
use Test::More;
use File::Temp qw/tempdir/;

#----------------------------------------------------------------------
# Test various errors / warnings / deprecations etc
#----------------------------------------------------------------------

BEGIN {
	eval "use DBD::SQLite";
	plan $@ ? (skip_all => 'needs DBD::SQLite for testing') : (tests => 22);
}

use File::Temp qw/tempfile/;
my (undef, $DB) = tempfile();
my @DSN = ("dbi:SQLite:dbname=$DB", '', '', { AutoCommit => 1 });

END { unlink $DB if -e $DB }

package Holiday;

use base 'Class::DBI';

{
	my $warning;
	local $SIG{__WARN__} = sub {
		$warning = $_[0];
	};
	no warnings 'once';
	local *UNIVERSAL::wonka = sub { };
	Holiday->columns(TEMP => 'wonka');
	::like $warning, qr/wonka.*clashes/, "Column clash warning for inherited";
	undef $warning;

	Holiday->columns(Primary => 'new');
	::like $warning, qr/new.*clashes/, "Column clash warning with CDBI";
	undef $warning;

	Holiday->add_constructor('by_train');
	Holiday::Camp->add_constructor('by_train');
	::is $warning, undef, "subclassed constructor";
}

{
	local $SIG{__WARN__} = sub {
		::like $_[0], qr/deprecated/, "create trigger deprecated";
	};
	Holiday->add_trigger('create' => sub { 1 });
	Holiday->add_trigger('delete' => sub { 1 });
}

{
	eval { Holiday->add_constraint };
	::like $@, qr/needs a name/, "Constraint with no name";
	eval { Holiday->add_constraint('check_mate') };
	::like $@, qr/needs a valid column/, "Constraint needs a column";
	eval { Holiday->add_constraint('check_mate', 'jamtart') };
	::like $@, qr/needs a valid column/, "No such column";
	eval { Holiday->add_constraint('check_mate', 'new') };
	::like $@, qr/needs a code ref/, "Need a coderef";
	eval { Holiday->add_constraint('check_mate', 'new', {}) };
	::like $@, qr/not a code ref/, "Not a coderef";

	eval { Holiday->has_a('new') };
	::like $@, qr/associated class/, "has_a needs a class";

	eval { Holiday->add_constructor() };
	::like $@, qr/name/, "add_constructor needs a method name";

	eval {
		Holiday->add_trigger(on_setting => sub { 1 });
	};
	::like $@, qr/no longer exists/, "No on_setting trigger";

	{
		local $SIG{__WARN__} = sub {
			::like $_[0], qr/new.*clashes/, "Column clash warning";
		};
	}
}

package main;

{
	package Holiday::Camp;
	use base 'Holiday';

	__PACKAGE__->table("holiday");
	__PACKAGE__->add_trigger(before_create => sub { 
		my $self = shift;
		$self->_croak("Problem with $self\n");
	});

	package main;
	eval { Holiday::Camp->insert({}) };
	like $@, qr/Problem with Holiday/, '$self stringifies with no PK values';
}

eval { my $foo = Holiday->retrieve({ id => 1 }) };
like $@, qr/retrieve a reference/, "Can't retrieve a reference";

eval { my $foo = Holiday->insert(id => 10) };
like $@, qr/a hashref/, "Can't create without hashref";

{
	my $foo = bless {}, 'Holiday';
	local $SIG{__WARN__} = sub { die $_[0] };
	eval { $foo->has_a(date => 'Date::Simple') };
	like $@, qr/object method/, "has_a is class-level: $@";
}

eval { Holiday->update; };
like $@, qr/class method/, "Can't call update as class method";

is(Holiday->table, 'holiday', "Default table name");

Holiday->_flesh('Blanket');

eval { Holiday->insert({ yonkey => 84 }) };
like $@, qr/not a column/, "Can't create with nonsense column";

eval { Film->_require_class('Class::DBI::__::Nonsense') };
like $@, qr/Can't locate/, "Can't require nonsense class";

eval { Holiday->search_DeleteMe };
like $@, qr/locate.*DeleteMe/, $@;

