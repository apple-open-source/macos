package DateTime::Format::Builder::Parser;
{
  $DateTime::Format::Builder::Parser::VERSION = '0.81';
}
use strict;
use warnings;
use Carp qw( croak );
use Params::Validate qw(
    validate SCALAR CODEREF UNDEF ARRAYREF
);
use Scalar::Util qw( weaken );




sub on_fail {
    my ( $self, $input, $parent ) = @_;
    my $maker = $self->maker;
    if ( $maker and $maker->can('on_fail') ) {
        $maker->on_fail($input);
    }
    else {
        croak __PACKAGE__ . ": Invalid date format: $input";
    }
}

sub no_parser {
    croak "No parser set for this parser object.";
}

sub new {
    my $class = shift;
    $class = ref($class) || $class;
    my $i    = 0;
    my $self = bless {
        on_fail => \&on_fail,
        parser  => \&no_parser,
    }, $class;

    return $self;
}

sub maker { $_[0]->{maker} }

sub set_maker {
    my $self  = shift;
    my $maker = shift;

    $self->{maker} = $maker;
    weaken $self->{maker}
        if ref $self->{maker};

    return $self;
}

sub fail {
    my ( $self, $parent, $input ) = @_;
    $self->{on_fail}->( $self, $input, $parent );
}

sub parse {
    my ( $self, $parent, $input, @args ) = @_;
    my $r = $self->{parser}->( $parent, $input, @args );
    $self->fail( $parent, $input ) unless defined $r;
    $r;
}

sub set_parser {
    my ( $self, $parser ) = @_;
    $self->{parser} = $parser;
    $self;
}

sub set_fail {
    my ( $self, $fail ) = @_;
    $self->{on_fail} = $fail;
    $self;
}


my @callbacks = qw( on_match on_fail postprocess preprocess );

{


    my %params = (
        common => {
            length => {
                type      => SCALAR | ARRAYREF,
                optional  => 1,
                callbacks => {
                    'is an int' => sub { ref $_[0] ? 1 : $_[0] !~ /\D/ },
                    'not empty' => sub { ref $_[0] ? @{ $_[0] } >= 1 : 1 },
                }
            },

            # Stuff used by callbacks
            label => { type => SCALAR, optional => 1 },
            (
                map { $_ => { type => CODEREF | ARRAYREF, optional => 1 } }
                    @callbacks
            ),
        },
    );


    sub params {
        my $self = shift;
        my $caller = ref $self || $self;
        return { map { %$_ } @params{ $caller, 'common' } };
    }


    my $all_params;

    sub params_all {
        return $all_params if defined $all_params;
        my %all_params = map { %$_ } values %params;
        $_->{optional} = 1 for values %all_params;
        $all_params = \%all_params;
    }


    my %inverse;

    sub valid_params {
        my $self = shift;
        my $from = (caller)[0];
        my %args = @_;
        $params{$from} = \%args;
        for ( keys %args ) {

            # %inverse contains keys matching all the
            # possible params; values are the class if and
            # only if that class is the only one that uses
            # the given param.
            $inverse{$_} = exists $inverse{$_} ? undef : $from;
        }
        undef $all_params;
        1;
    }


    sub whose_params {
        my $param = shift;
        return $inverse{$param};
    }
}


sub create_single_object {
    my ($self) = shift;
    my $obj    = $self->new;
    my $parser = $self->create_single_parser(@_);

    $obj->set_parser($parser);
}

sub create_single_parser {
    my $class = shift;
    return $_[0] if ref $_[0] eq 'CODE';    # already code
    @_ = %{ $_[0] } if ref $_[0] eq 'HASH'; # turn hashref into hash
                                            # ordinary boring sort
    my %args = validate( @_, params_all() );

    # Determine variables for ease of reference.
    for (@callbacks) {
        $args{$_} = $class->merge_callbacks( $args{$_} ) if $args{$_};
    }

    # Determine parser class
    my $from;
    for ( keys %args ) {
        $from = whose_params($_);
        next if ( not defined $from ) or ( $from eq 'common' );
        last;
    }
    croak "Could not identify a parsing module to use." unless $from;

    # Find and call parser creation method
    my $method = $from->can("create_parser")
        or croak
        "Can't create a $_ parser (no appropriate create_parser method)";
    my @args = %args;
    %args = validate( @args, $from->params() );
    $from->$method(%args);
}


sub merge_callbacks {
    my $self = shift;

    return unless @_;       # No arguments
    return unless $_[0];    # Irrelevant argument
    my @callbacks = @_;
    if ( @_ == 1 ) {
        return $_[0] if ref $_[0] eq 'CODE';
        @callbacks = @{ $_[0] } if ref $_[0] eq 'ARRAY';
    }
    return unless @callbacks;

    for (@callbacks) {
        croak "All callbacks must be coderefs!" unless ref $_ eq 'CODE';
    }

    return sub {
        my $rv;
        my %args = @_;
        for my $cb (@callbacks) {
            $rv = $cb->(%args);
            return $rv unless $rv;

            # Ugh. Symbiotic. All but postprocessor return the date.
            $args{input} = $rv unless $args{parsed};
        }
        $rv;
    };
}


sub create_multiple_parsers {
    my $class = shift;
    my ( $options, @specs ) = @_;

    my $obj = $class->new;

    # Organise the specs, and transform them into parsers.
    my ( $lengths, $others ) = $class->sort_parsers( $options, \@specs );

    # Merge callbacks if any.
    for ('preprocess') {
        $options->{$_} = $class->merge_callbacks( $options->{$_} )
            if $options->{$_};
    }

    # Custom fail method?
    $obj->set_fail( $options->{on_fail} ) if exists $options->{on_fail};

    # Who's our maker?
    $obj->set_maker( $options->{maker} ) if exists $options->{maker};

    # We don't want to save the whole options hash as a closure, since
    # that can cause a circular reference when $options->{maker} is
    # set.
    my $preprocess = $options->{preprocess};

    # These are the innards of a multi-parser.
    my $parser = sub {
        my ( $self, $date, @args ) = @_;
        return unless defined $date;

        # Parameters common to the callbacks. Pre-prepared.
        my %param = (
            self => $self,
            ( @args ? ( args => \@args ) : () ),
        );

        my %p;

        # Preprocess and potentially fill %p
        if ($preprocess) {
            $date = $preprocess->( input => $date, parsed => \%p, %param );
        }

        # Find length parser
        if (%$lengths) {
            my $length = length $date;
            my $parser = $lengths->{$length};
            if ($parser) {

                # Found one, call it with _copy_ of %p
                my $dt = $parser->( $self, $date, {%p}, @args );
                return $dt if defined $dt;
            }
        }

        # Or calls all others, with _copy_ of %p
        for my $parser (@$others) {
            my $dt = $parser->( $self, $date, {%p}, @args );
            return $dt if defined $dt;
        }

        # Failed, return undef.
        return;
    };
    $obj->set_parser($parser);
}


sub sort_parsers {
    my $class = shift;
    my ( $options, $specs ) = @_;
    my ( %lengths, @others );

    for my $spec (@$specs) {

        # Put coderefs straight into the 'other' heap.
        if ( ref $spec eq 'CODE' ) {
            push @others, $spec;
        }

        # Specifications...
        elsif ( ref $spec eq 'HASH' ) {
            if ( exists $spec->{length} ) {
                my $code = $class->create_single_parser(%$spec);
                my @lengths
                    = ref $spec->{length}
                    ? @{ $spec->{length} }
                    : ( $spec->{length} );
                for my $length (@lengths) {
                    push @{ $lengths{$length} }, $code;
                }
            }
            else {
                push @others, $class->create_single_parser(%$spec);
            }
        }

        # Something else
        else {
            croak "Invalid specification in list.";
        }
    }

    while ( my ( $length, $parsers ) = each %lengths ) {
        $lengths{$length} = $class->chain_parsers($parsers);
    }

    return ( \%lengths, \@others );
}

sub chain_parsers {
    my ( $self, $parsers ) = @_;
    return $parsers->[0] if @$parsers == 1;
    return sub {
        my $self = shift;
        for my $parser (@$parsers) {
            my $rv = $self->$parser(@_);
            return $rv if defined $rv;
        }
        return undef;
    };
}


sub create_parser {
    my $class = shift;
    if ( not ref $_[0] ) {

        # Simple case of single specification as a hash
        return $class->create_single_object(@_);
    }

    # Let's see if we were given an options block
    my %options;
    while ( ref $_[0] eq 'ARRAY' ) {
        my $options = shift;
        %options = ( %options, @$options );
    }

    # Now, can we create a multi-parser out of the remaining arguments?
    if ( ref $_[0] eq 'HASH' or ref $_[0] eq 'CODE' ) {
        return $class->create_multiple_parsers( \%options, @_ );
    }
    else {
        # If it wasn't a HASH or CODE, then it was (ideally)
        # a list of pairs describing a single specification.
        return $class->create_multiple_parsers( \%options, {@_} );
    }
}


# Find all our workers
{
    use Class::Factory::Util 1.6;

    foreach my $worker ( __PACKAGE__->subclasses ) {
        eval "use DateTime::Format::Builder::Parser::$worker;";
        die $@ if $@;
    }
}

1;

# ABSTRACT: Parser creation

__END__

=pod

=head1 NAME

DateTime::Format::Builder::Parser - Parser creation

=head1 VERSION

version 0.81

=head1 SYNOPSIS

    my $class = 'DateTime::Format::Builder::Parser';
    my $parser = $class->create_single_parser( %specs );

=head1 DESCRIPTION

This is a utility class for L<DateTime::Format::Builder> that
handles creation of parsers. It is to here that C<Builder> delegates
most of its responsibilities.

=head1 CONSTRUCTORS

=head1 METHODS

There are two sorts of methods in this class. Those used by
parser implementations and those used by C<Builder>. It is
generally unlikely the user will want to use any of them.

They are presented, grouped according to use.

=head2 Parameter Handling (implementations)

These methods allow implementations to have validation of
their arguments in a standard manner and due to C<Parser>'s
impelementation, these methods also allow C<Parser> to
determine which implementation to use.

=head3 Common parameters

These parameters appear for all parser implementations.
These are primarily documented in
L<DateTime::Format::Builder>.

=over 4

=item *

B<on_match>

=item *

B<on_fail>

=item *

B<postprocess>

=item *

B<preprocess>

=item *

B<label>

=item *

B<length> may be a number or an arrayref of numbers
indicating the length of the input. This lets us optimise in
the case of static length input. If supplying an arrayref of
numbers, please keep the number of numbers to a minimum.

=back

=head3 params

    my $params = $self->params();
    validate( @_, $params );

Returns declared parameters and C<common> parameters in a hashref
suitable for handing to L<Params::Validate>'s C<validate> function.

=head3 params_all

    my $all_params = $self->params_all();

Returns a hash of all the valid options. Not recommended
for general use.

=head3 valid_params

    __PACKAGE__->valid_params( %params );

Arguments are as per L<Params::Validate>'s C<validate> function.
This method is used to declare what your valid arguments are in
a parser specification.

=head3 whose_params

    my $class = whose_params( $key );

Internal function which merely returns to which class a
parameter is unique. If not unique, returns C<undef>.

=head2 Organising and Creating Parsers

=head3 create_single_parser

This takes a single specification and returns a coderef that
is a parser that suits that specification. This is the end
of the line for all the parser creation methods. It
delegates no further.

If a coderef is specified, then that coderef is immediately
returned (it is assumed to be appropriate).

The single specification (if not a coderef) can be either a
hashref or a hash. The keys and values must be as per the
specification.

It is here that any arrays of callbacks are unified. It is
also here that any parser implementations are used. With
the spec that's given, the keys are looked at and whichever
module is the first to have a unique key in the spec is the
one to whom the spec is given.

B<Note>: please declare a C<valid_params> argument with an
uppercase letter. For example, if you're writing
C<DateTime::Format::Builder::Parser::Fnord>, declare a
parameter called C<Fnord>. Similarly, C<DTFBP::Strptime>
should have C<Strptime> and C<DTFBP::Regex> should have
C<Regex>. These latter two don't for backwards compatibility
reasons.

The returned parser will return either a C<DateTime> object
or C<undef>.

=head3 merge_callbacks

Produce either undef or a single coderef from either undef,
an empty array, a single coderef or an array of coderefs

=head2 create_multiple_parsers

Given the options block (as made from C<create_parser()>)
and a list of single parser specifications, this returns a
coderef that returns either the resultant C<DateTime> object
or C<undef>.

It first sorts the specifications using C<sort_parsers()>
and then creates the function based on what that returned.

=head2 sort_parsers

This takes the list of specifications and sorts them while
turning the specifications into parsers. It returns two
values: the first is a hashref containing all the length
based parsers. The second is an array containing all the
other parsers.

If any of the specs are not code or hash references, then it
will call C<croak()>.

Code references are put directly into the 'other' array. Any
hash references without I<length> keys are run through
C<create_single_parser()> and the resultant parser is placed
in the 'other' array.

Hash references B<with> I<length> keys are run through
C<create_single_parser()>, but the resultant parser is used
as the value in the length hashref with the length being the
key. If two or more parsers have the same I<length>
specified then an error is thrown.

=head2 create_parser

C<create_class()> is mostly a wrapper around
C<create_parser()> that does loops and stuff and calls
C<create_parser()> to create the actual parsers.

C<create_parser()> takes the parser specifications (be they
single specifications or multiple specifications) and
returns an anonymous coderef that is suitable for use as a
method. The coderef will call C<croak()> in the event of
being unable to parse the single string it expects as input.

The simplest input is that of a single specification,
presented just as a plain hash, not a hashref. This is
passed directly to C<create_single_parser()> with the return
value from that being wrapped in a function that lets it
C<croak()> on failure, with that wrapper being returned.

If the first argument to C<create_parser()> is an arrayref,
then that is taken to be an options block (as per the
multiple parser specification documented earlier).

Any further arguments should be either hashrefs or coderefs.
If the first argument after the optional arrayref is not a
hashref or coderef then that argument and all remaining
arguments are passed off to C<create_single_parser()>
directly. If the first argument is a hashref or coderef,
then it and the remaining arguments are passed to
C<create_multiple_parsers()>.

The resultant coderef from calling either of the creation
methods is then wrapped in a function that calls C<croak()>
in event of failure or the C<DateTime> object in event of
success.

=head1 FINDING IMPLEMENTATIONS

C<Parser> automatically loads any parser classes in C<@INC>.

To be loaded automatically, you must be a
C<DateTime::Format::Builder::Parser::XXX> module.

To be invisible, and not loaded, start your class with a lower class
letter. These are ignored.

=head1 WRITING A PARSER IMPLEMENTATION

=head2 Naming your parser

Create a module and name it in the form
C<DateTime::Format::Builder::Parser::XXX>
where I<XXX> is whatever you like,
so long as it doesn't start with a
lower case letter.

Alternatively, call it something completely different
if you don't mind the users explicitly loading your module.

I'd recommend keeping within the C<DateTime::Format::Builder>
namespace though --- at the time of writing I've not given
thought to what non-auto loaded ones should be called. Any
ideas, please email me.

=head2 Declaring specification arguments

Call C<<DateTime::Format::Builder::Parser->valid_params()>> with
C<Params::Validate> style arguments. For example:

   DateTime::Format::Builder::Parser->valid_params(
       params => { type => ARRAYREF },
       Regex  => { type => SCALARREF, callbacks => {
          'is a regex' => sub { ref(shift) eq 'Regexp' }
       }}
   );

Start one of the key names with a capital letter. Ideally that key
should match the I<XXX> from earlier. This will be used to help
identify which module a parser specification should be given to.

The key names I<on_match>, I<on_fail>, I<postprocess>, I<preprocess>,
I<label> and I<length> are predefined. You are recommended to make use
of them. You may ignore I<length> as C<sort_parsers> takes care of that.

=head2 Define create_parser

A class method of the name C<create_parser> that does the following:

Its arguments are as for a normal method (i.e. class as first argument).
The other arguments are the result from a call to C<Params::Validate>
according to your specification (the C<valid_params> earlier), i.e. a
hash of argument name and value.

The return value should be a coderef that takes a date string as its
first argument and returns either a C<DateTime> object or C<undef>.

=head2 Callbacks

It is preferred that you support some callbacks to your parsers.
In particular, C<preprocess>, C<on_match>, C<on_fail> and
C<postprocess>. See the L<main Builder|DateTime::Format::Builder>
docs for the appropriate placing of calls to the callbacks.

=head1 SUPPORT

See L<DateTime::Format::Builder> for details.

=head1 SEE ALSO

C<datetime@perl.org> mailing list.

http://datetime.perl.org/

L<perl>, L<DateTime>, L<DateTime::Format::Builder>.

L<Params::Validate>.

L<DateTime::Format::Builder::Parser::generic>,
L<DateTime::Format::Builder::Parser::Dispatch>,
L<DateTime::Format::Builder::Parser::Quick>,
L<DateTime::Format::Builder::Parser::Regex>,
L<DateTime::Format::Builder::Parser::Strptime>.

=head1 AUTHORS

=over 4

=item *

Dave Rolsky <autarch@urth.org>

=item *

Iain Truskett

=back

=head1 COPYRIGHT AND LICENSE

This software is Copyright (c) 2013 by Dave Rolsky.

This is free software, licensed under:

  The Artistic License 2.0 (GPL Compatible)

=cut
