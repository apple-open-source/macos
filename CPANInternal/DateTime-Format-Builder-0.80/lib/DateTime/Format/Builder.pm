package DateTime::Format::Builder;
# $Id: Builder.pm 4400 2010-03-14 15:49:10Z autarch $

=begin comments

Note: there is no API documentation in this file. You want F<Builder.pod> instead.

=cut

use strict;
use 5.005;
use Carp;
use DateTime 0.12;
use Params::Validate qw(
    validate SCALAR ARRAYREF HASHREF SCALARREF CODEREF GLOB GLOBREF UNDEF
);
use vars qw( $VERSION %dispatch_data );

my $parser = 'DateTime::Format::Builder::Parser';
$VERSION = '0.80';

# Developer oriented methods

=pod

C<verbose()> sets the logging.

=cut

sub verbose
{
    warn "Use of verbose() deprecated for the interim.";
    1;
}

=pod

C<import()> merely exists to save typing. class is specified after C<@_>
in order to override it. We really don't want to know about
any class they specify. We'd leave it empty, but C<create_class()>
uses C<caller()> to determine where the code came from.

=cut

sub import
{
    my $class = shift;
    $class->create_class( @_, class => (caller)[0] ) if @_;
}

=pod

Populates C<$class::VERSION>, C<$class::new> and writes any
of the methods.

=cut

sub create_class
{
    my $class = shift;
    my %args = validate( @_, {
	class	=> { type => SCALAR, default => (caller)[0] },
	version => { type => SCALAR, optional => 1 },
	verbose	=> { type => SCALAR|GLOBREF|GLOB, optional => 1 },
	parsers	=> { type => HASHREF },
	groups  => { type => HASHREF, optional => 1 },
	constructor => { type => UNDEF|SCALAR|CODEREF, optional => 1 },
    });

    verbose( $args{verbose} ) if exists $args{verbose};

    my $target = $args{class}; # where we're writing our methods and such.

    # Create own lovely new package
    {
	no strict 'refs';


	${"${target}::VERSION"} = $args{version} if exists $args{version};

	$class->create_constructor(
	    $target, exists $args{constructor}, $args{constructor} );

	# Turn groups of parser specs in to groups of parsers
	{
	    my $specs = $args{groups};
	    my %groups;

	    for my $label ( keys %$specs )
	    {
		my $parsers = $specs->{$label};
		my $code = $class->create_parser( $parsers );
		$groups{$label} = $code;
	    }

	    $dispatch_data{$target} = \%groups;
	}

	# Write all our parser methods, creating parsers as we go.
	while (my ($method, $parsers) = each %{ $args{parsers} })
	{
	    my $globname = $target."::$method";
 	    croak "Will not override a preexisting method $method()" if defined &{$globname};
	    *$globname = $class->create_end_parser( $parsers );
	}
    }

}

sub create_constructor
{
    my $class = shift;
    my ( $target, $intended, $value ) = @_;

    my $new = $target."::new";
    $value = 1 unless $intended;

    return unless $value;
    return if not $intended and defined &$new;
    croak "Will not override a preexisting constructor new()" if defined &$new;

    no strict 'refs';

    return *$new = $value if ref $value eq 'CODE';
    return *$new = sub {
	my $class = shift;
 	croak "${class}->new takes no parameters." if @_;

	my $self = bless {}, ref($class)||$class;
	# If called on an object, clone, but we've nothing to
	# clone

	$self;
    };
}

=pod

This creates the parser coderefs. Coderefs return undef on
bad parses, return C<DateTime> objects on good parse. Used
by C<parser()> and C<create_class()>.

=cut

sub create_parser
{
    my $class = shift;
    my @common = ( maker => $class );
    if (@_ == 1)
    {
	my $parsers = shift;
	my @parsers = (
	    (ref $parsers eq 'HASH' ) ? %$parsers :
	    ( ( ref $parsers eq 'ARRAY' ) ? @$parsers : $parsers)
	);
	$parser->create_parser( \@common, @parsers );
    }
    else
    {
	$parser->create_parser( \@common, @_ );
    }
}

=pod

This creates the end methods. Coderefs die on bad parses,
return C<DateTime> objects on good parse.

=cut

sub create_end_parser
{
    my ($class, $parsers) = @_;
    $class->create_method( $class->create_parser( $parsers ) );
}

=pod

C<create_method()> simply takes a parser and returns a coderef suitable
to act as a method.

=cut

sub create_method
{
    my ($class, $parser) = @_;
    return sub {
	my $self = shift;
	$parser->parse( $self, @_);
    }
}

=pod

This is the method used when a parse fails. Subclass and override
this if you like.

=cut

sub on_fail
{
    my ($class, $input) = @_;

    my $pkg;
    my $i = 0;
    while (($pkg) = caller($i++)) {
        last if (!UNIVERSAL::isa($pkg, 'DateTime::Format::Builder') &&
            !UNIVERSAL::isa($pkg, 'DateTime::Format::Builder::Parser'));
    }
    local $Carp::CarpLevel = $i;
    croak "Invalid date format: $input";
}

#
# User oriented methods
#

=pod

These methods don't need explaining. They're pretty much
boiler plate stuff.

=cut

sub new
{
    my $class = shift;
    croak "Constructor 'new' takes no parameters" if @_;
    my $self = bless {
	parser => sub { croak "No parser set." }
    }, ref($class)||$class;
    if (ref $class)
    {
	# If called on an object, clone
	$self->set_parser( $class->get_parser );
	# and that's it. we don't store that much info per object
    }
    return $self;
}

sub parser
{
    my $class = shift;
    my $parser = $class->create_end_parser( \@_ );

    # Do we need to instantiate a new object for return,
    # or are we modifying an existing object?
    my $self;
    $self = ref $class ? $class : $class->new();

    $self->set_parser( $parser );

    $self;
}

sub clone
{
    my $self = shift;
    croak "Calling object method as class method!" unless ref $self;
    return $self->new();
}

sub set_parser
{
    my ($self, $parser) = @_;
    croak "set_parser given something other than a coderef" unless $parser
	and ref $parser eq 'CODE';
    $self->{parser} = $parser;
    $self;
}

sub get_parser
{
    my ($self) = @_;
    return $self->{parser};
}

sub parse_datetime
{
    my $self = shift;
    croak "parse_datetime is an object method, not a class method."
        unless ref $self and $self->isa( __PACKAGE__ );
    croak "No date specified." unless @_;
    return $self->{parser}->( $self, @_ );
}

sub format_datetime
{
    croak __PACKAGE__."::format_datetime not implemented.";
}

require DateTime::Format::Builder::Parser;


=pod

Create the single parser. Delegation stops here!

=cut

1;
