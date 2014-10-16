package DateTime::Format::Builder;
{
  $DateTime::Format::Builder::VERSION = '0.81';
}

use strict;
use warnings;

use 5.005;
use Carp;
use DateTime 1.00;
use Params::Validate 0.72 qw(
    validate SCALAR ARRAYREF HASHREF SCALARREF CODEREF GLOB GLOBREF UNDEF
);
use vars qw( %dispatch_data );

my $parser = 'DateTime::Format::Builder::Parser';

sub verbose {
    warn "Use of verbose() deprecated for the interim.";
    1;
}

sub import {
    my $class = shift;
    $class->create_class( @_, class => (caller)[0] ) if @_;
}

sub create_class {
    my $class = shift;
    my %args  = validate(
        @_,
        {
            class   => { type => SCALAR, default  => (caller)[0] },
            version => { type => SCALAR, optional => 1 },
            verbose => { type => SCALAR | GLOBREF | GLOB, optional => 1 },
            parsers => { type => HASHREF },
            groups  => { type => HASHREF,                 optional => 1 },
            constructor =>
                { type => UNDEF | SCALAR | CODEREF, optional => 1 },
        }
    );

    verbose( $args{verbose} ) if exists $args{verbose};

    my $target = $args{class};    # where we're writing our methods and such.

    # Create own lovely new package
    {
        no strict 'refs';

        ${"${target}::VERSION"} = $args{version} if exists $args{version};

        $class->create_constructor(
            $target, exists $args{constructor},
            $args{constructor}
        );

        # Turn groups of parser specs in to groups of parsers
        {
            my $specs = $args{groups};
            my %groups;

            for my $label ( keys %$specs ) {
                my $parsers = $specs->{$label};
                my $code    = $class->create_parser($parsers);
                $groups{$label} = $code;
            }

            $dispatch_data{$target} = \%groups;
        }

        # Write all our parser methods, creating parsers as we go.
        while ( my ( $method, $parsers ) = each %{ $args{parsers} } ) {
            my $globname = $target . "::$method";
            croak "Will not override a preexisting method $method()"
                if defined &{$globname};
            *$globname = $class->create_end_parser($parsers);
        }
    }

}

sub create_constructor {
    my $class = shift;
    my ( $target, $intended, $value ) = @_;

    my $new = $target . "::new";
    $value = 1 unless $intended;

    return unless $value;
    return if not $intended and defined &$new;
    croak "Will not override a preexisting constructor new()"
        if defined &$new;

    no strict 'refs';

    return *$new = $value if ref $value eq 'CODE';
    return *$new = sub {
        my $class = shift;
        croak "${class}->new takes no parameters." if @_;

        my $self = bless {}, ref($class) || $class;

        # If called on an object, clone, but we've nothing to
        # clone

        $self;
    };
}

sub create_parser {
    my $class = shift;
    my @common = ( maker => $class );
    if ( @_ == 1 ) {
        my $parsers = shift;
        my @parsers = (
            ( ref $parsers eq 'HASH' )
            ? %$parsers
            : ( ( ref $parsers eq 'ARRAY' ) ? @$parsers : $parsers )
        );
        $parser->create_parser( \@common, @parsers );
    }
    else {
        $parser->create_parser( \@common, @_ );
    }
}


sub create_end_parser {
    my ( $class, $parsers ) = @_;
    $class->create_method( $class->create_parser($parsers) );
}

sub create_method {
    my ( $class, $parser ) = @_;
    return sub {
        my $self = shift;
        $parser->parse( $self, @_ );
        }
}

sub on_fail {
    my ( $class, $input ) = @_;

    my $pkg;
    my $i = 0;
    while ( ($pkg) = caller( $i++ ) ) {
        last
            if ( !UNIVERSAL::isa( $pkg, 'DateTime::Format::Builder' )
            && !UNIVERSAL::isa( $pkg, 'DateTime::Format::Builder::Parser' ) );
    }
    local $Carp::CarpLevel = $i;
    croak "Invalid date format: $input";
}

sub new {
    my $class = shift;
    croak "Constructor 'new' takes no parameters" if @_;
    my $self = bless {
        parser => sub { croak "No parser set." }
        },
        ref($class) || $class;
    if ( ref $class ) {

        # If called on an object, clone
        $self->set_parser( $class->get_parser );

        # and that's it. we don't store that much info per object
    }
    return $self;
}

sub parser {
    my $class  = shift;
    my $parser = $class->create_end_parser( \@_ );

    # Do we need to instantiate a new object for return,
    # or are we modifying an existing object?
    my $self;
    $self = ref $class ? $class : $class->new();

    $self->set_parser($parser);

    $self;
}

sub clone {
    my $self = shift;
    croak "Calling object method as class method!" unless ref $self;
    return $self->new();
}

sub set_parser {
    my ( $self, $parser ) = @_;
    croak "set_parser given something other than a coderef"
        unless $parser
        and ref $parser eq 'CODE';
    $self->{parser} = $parser;
    $self;
}

sub get_parser {
    my ($self) = @_;
    return $self->{parser};
}

sub parse_datetime {
    my $self = shift;
    croak "parse_datetime is an object method, not a class method."
        unless ref $self and $self->isa(__PACKAGE__);
    croak "No date specified." unless @_;
    return $self->{parser}->( $self, @_ );
}

sub format_datetime {
    croak __PACKAGE__ . "::format_datetime not implemented.";
}

require DateTime::Format::Builder::Parser;

1;

# ABSTRACT: Create DateTime parser classes and objects.

__END__

=pod

=head1 NAME

DateTime::Format::Builder - Create DateTime parser classes and objects.

=head1 VERSION

version 0.81

=head1 SYNOPSIS

    package DateTime::Format::Brief;

    use DateTime::Format::Builder
    (
        parsers => {
            parse_datetime => [
            {
                regex => qr/^(\d{4})(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)$/,
                params => [qw( year month day hour minute second )],
            },
            {
                regex => qr/^(\d{4})(\d\d)(\d\d)$/,
                params => [qw( year month day )],
            },
            ],
        }
    );

=head1 DESCRIPTION

DateTime::Format::Builder creates DateTime parsers.
Many string formats of dates and times are simple and just
require a basic regular expression to extract the relevant
information. Builder provides a simple way to do this
without writing reams of structural code.

Builder provides a number of methods, most of which you'll
never need, or at least rarely need. They're provided more
for exposing of the module's innards to any subclasses, or
for when you need to do something slightly beyond what I
expected.

This creates the end methods. Coderefs die on bad parses,
return C<DateTime> objects on good parse.

=head1 TUTORIAL

See L<DateTime::Format::Builder::Tutorial>.

=head1 ERROR HANDLING AND BAD PARSES

Often, I will speak of C<undef> being returned, however
that's not strictly true.

When a simple single specification is given for a method,
the method isn't given a single parser directly. It's given
a wrapper that will call C<on_fail()> if the single parser
returns C<undef>. The single parser must return C<undef> so
that a multiple parser can work nicely and actual errors can
be thrown from any of the callbacks.

Similarly, any multiple parsers will only call C<on_fail()>
right at the end when it's tried all it could.

C<on_fail()> (see L<later|/on_fail>) is defined, by default,
to throw an error.

Multiple parser specifications can also specify C<on_fail>
with a coderef as an argument in the options block. This
will take precedence over the inheritable and over-ridable
method.

That said, don't throw real errors from callbacks in
multiple parser specifications unless you really want
parsing to stop right there and not try any other parsers.

In summary: calling a B<method> will result in either a
C<DateTime> object being returned or an error being thrown
(unless you've overridden C<on_fail()> or
C<create_method()>, or you've specified a C<on_fail> key to
a multiple parser specification).

Individual B<parsers> (be they multiple parsers or single
parsers) will return either the C<DateTime> object or
C<undef>.

=head1 SINGLE SPECIFICATIONS

A single specification is a hash ref of instructions
on how to create a parser.

The precise set of keys and values varies according to parser
type. There are some common ones though:

=over 4

=item *

B<length> is an optional parameter that can be used to
specify that this particular I<regex> is only applicable to
strings of a certain fixed length. This can be used to make
parsers more efficient. It's strongly recommended that any
parser that can use this parameter does.

You may happily specify the same length twice. The parsers
will be tried in order of specification.

You can also specify multiple lengths by giving it an
arrayref of numbers rather than just a single scalar.
If doing so, please keep the number of lengths to a minimum.

If any specifications without I<length>s are given and the
particular I<length> parser fails, then the non-I<length>
parsers are tried.

This parameter is ignored unless the specification is part
of a multiple parser specification.

=item *

B<label> provides a name for the specification and is passed
to some of the callbacks about to mentioned.

=item *

B<on_match> and B<on_fail> are callbacks. Both routines will
be called with parameters of:

=over 4

=item *

B<input>, being the input to the parser (after any
preprocessing callbacks).

=item *

B<label>, being the label of the parser, if there is one.

=item *

B<self>, being the object on which the method has been
invoked (which may just be a class name). Naturally, you
can then invoke your own methods on it do get information
you want.

=item *

B<args>, being an arrayref of any passed arguments, if any.
If there were no arguments, then this parameter is not given.

=back

These routines will be called depending on whether the
B<regex> match succeeded or failed.

=item *

B<preprocess> is a callback provided for cleaning up input
prior to parsing. It's given a hash as arguments with the
following keys:

=over 4

=item *

B<input> being the datetime string the parser was given (if
using multiple specifications and an overall I<preprocess>
then this is the date after it's been through that
preprocessor).

=item *

B<parsed> being the state of parsing so far. Usually empty
at this point unless an overall I<preprocess> was given.
Items may be placed in it and will be given to any
B<postprocess>or and C<< DateTime->new >> (unless the
postprocessor deletes it).

=item *

B<self>, B<args>, B<label> as per I<on_match> and I<on_fail>.

=back

The return value from the routine is what is given to the
I<regex>. Note that this is last code stop before the match.

B<Note>: mixing I<length> and a I<preprocess> that modifies
the length of the input string is probably not what you
meant to do. You probably meant to use the
I<multiple parser> variant of I<preprocess> which is done
B<before> any length calculations. This C<single parser> variant
of I<preprocess> is performed B<after> any length
calculations.

=item *

B<postprocess> is the last code stop before
C<< DateTime->new() >> is called. It's given the same
arguments as I<preprocess>. This allows it to modify the
parsed parameters after the parse and before the creation
of the object. For example, you might use:

    {
        regex  => qr/^(\d\d) (\d\d) (\d\d)$/,
	params => [qw( year  month  day   )],
	postprocess => \&_fix_year,
    }

where C<_fix_year> is defined as:

    sub _fix_year
    {
        my %args = @_;
	my ($date, $p) = @args{qw( input parsed )};
	$p->{year} += $p->{year} > 69 ? 1900 : 2000;
	return 1;
    }

This will cause the two digit years to be corrected
according to the cut off. If the year was '69' or lower,
then it is made into 2069 (or 2045, or whatever the year was
parsed as). Otherwise it is assumed to be 19xx. The
L<DateTime::Format::Mail> module uses code similar to this
(only it allows the cut off to be configured and it doesn't
use Builder).

B<Note>: It is B<very important> to return an explicit value
from the I<postprocess> callback. If the return value is
false then the parse is taken to have failed. If the return
value is true, then the parse is taken to have succeeded and
C<< DateTime->new() >> is called.

=back

See the documentation for the individual parsers for their
valid keys.

Parsers at the time of writing are:

=over 4

=item *

L<DateTime::Format::Builder::Parser::Regex> - provides regular
expression based parsing.

=item *

L<DateTime::Format::Builder::Parser::Strptime> - provides strptime
based parsing.

=back

=head2 Subroutines / coderefs as specifications.

A single parser specification can be a coderef. This was
added mostly because it could be and because I knew someone,
somewhere, would want to use it.

If the specification is a reference to a piece of code, be
it a subroutine, anonymous, or whatever, then it's passed
more or less straight through. The code should return
C<undef> in event of failure (or any false value,
but C<undef> is strongly preferred), or a true value in the
event of success (ideally a C<DateTime> object or some
object that has the same interface).

This all said, I generally wouldn't recommend using this
feature unless you have to.

=head2 Callbacks

I mention a number of callbacks in this document.

Any time you see a callback being mentioned, you can,
if you like, substitute an arrayref of coderefs rather
than having the straight coderef.

=head1 MULTIPLE SPECIFICATIONS

These are very easily described as an array of single
specifications.

Note that if the first element of the array is an arrayref,
then you're specifying options.

=over 4

=item *

B<preprocess> lets you specify a preprocessor that is called
before any of the parsers are tried. This lets you do things
like strip off timezones or any unnecessary data. The most
common use people have for it at present is to get the input
date to a particular length so that the I<length> is usable
(L<DateTime::Format::ICal> would use it to strip off the
variable length timezone).

Arguments are as for the I<single parser> I<preprocess>
variant with the exception that I<label> is never given.

=item *

B<on_fail> should be a reference to a subroutine that is
called if the parser fails. If this is not provided, the
default action is to call
C<DateTime::Format::Builder::on_fail>, or the C<on_fail>
method of the subclass of DTFB that was used to create the
parser.

=back

=head1 EXECUTION FLOW

Builder allows you to plug in a fair few callbacks, which
can make following how a parse failed (or succeeded
unexpectedly) somewhat tricky.

=head2 For Single Specifications

A single specification will do the following:

User calls parser:

       my $dt = $class->parse_datetime( $string );

=over 4

=item 1

I<preprocess> is called. It's given C<$string> and a
reference to the parsing workspace hash, which we'll call
C<$p>. At this point, C<$p> is empty. The return value is
used as C<$date> for the rest of this single parser.
Anything put in C<$p> is also used for the rest of this
single parser.

=item 2

I<regex> is applied.

=item 3

If I<regex> B<did not> match, then I<on_fail> is called (and is given
C<$date> and also I<label> if it was defined). Any return
value is ignored and the next thing is for the single
parser to return C<undef>.

If I<regex> B<did> match, then I<on_match> is called with
the same arguments as would be given to I<on_fail>. The
return value is similarly ignored, but we then move to step
4 rather than exiting the parser.

=item 4

I<postprocess> is called with C<$date> and a filled out
C<$p>. The return value is taken as a indication of whether
the parse was a success or not. If it wasn't a success then
the single parser will exit at this point, returning undef.

=item 5

C<< DateTime->new() >> is called and the user is given the
resultant C<DateTime> object.

=back

See the section on L<error handling|/"ERROR HANDLING AND BAD PARSES">
regarding the C<undef>s mentioned above.

=head2 For Multiple Specifications

With multiple specifications:

User calls parser:

      my $dt = $class->complex_parse( $string );

=over 4

=item 1

The overall I<preprocess>or is called and is given C<$string>
and the hashref C<$p> (identically to the per parser
I<preprocess> mentioned in the previous flow).

If the callback modifies C<$p> then a B<copy> of C<$p> is
given to each of the individual parsers.  This is so parsers
won't accidentally pollute each other's workspace.

=item 2

If an appropriate length specific parser is found, then it
is called and the single parser flow (see the previous
section) is followed, and the parser is given a copy of
C<$p> and the return value of the overall I<preprocess>or as
C<$date>.

If a C<DateTime> object was returned so we go straight back
to the user.

If no appropriate parser was found, or the parser returned
C<undef>, then we progress to step 3!

=item 3

Any non-I<length> based parsers are tried in the order they
were specified.

For each of those the single specification flow above is
performed, and is given a copy of the output from the
overall preprocessor.

If a real C<DateTime> object is returned then we exit back
to the user.

If no parser could parse, then an error is thrown.

=back

See the section on L<error handling|/ERROR HANDLING AND BAD PARSES>
regarding the C<undef>s mentioned above.

=head1 METHODS

In the general course of things you won't need any of the
methods. Life often throws unexpected things at us so the
methods are all available for use.

=head2 import

C<import()> is a wrapper for C<create_class()>. If you
specify the I<class> option (see documentation for
C<create_class()>) it will be ignored.

=head2 create_class

This method can be used as the runtime equivalent of
C<import()>. That is, it takes the exact same parameters as
when one does:

   use DateTime::Format::Builder ( blah blah blah )

That can be (almost) equivalently written as:

   use DateTime::Format::Builder;
   DateTime::Format::Builder->create_class( blah blah blah );

The difference being that the first is done at compile time
while the second is done at run time.

In the tutorial I said there were only two parameters at
present. I lied. There are actually three of them.

=over 4

=item *

B<parsers> takes a hashref of methods and their parser
specifications. See the
L<DateTime::Format::Builder::Tutorial> for details.

Note that if you define a subroutine of the same name as one
of the methods you define here, an error will be thrown.

=item *

B<constructor> determines whether and how to create a
C<new()> function in the new class. If given a true value, a
constructor is created. If given a false value, one isn't.

If given an anonymous sub or a reference to a sub then that
is used as C<new()>.

The default is C<1> (that is, create a constructor using
our default code which simply creates a hashref and blesses
it).

If your class defines its own C<new()> method it will not be
overwritten. If you define your own C<new()> and B<also> tell
Builder to define one an error will be thrown.

=item *

B<verbose> takes a value. If the value is undef, then
logging is disabled. If the value is a filehandle then
that's where logging will go. If it's a true value, then
output will go to C<STDERR>.

Alternatively, call C<$DateTime::Format::Builder::verbose()>
with the relevant value. Whichever value is given more
recently is adhered to.

Be aware that verbosity is a global wide setting.

=item *

B<class> is optional and specifies the name of the class in
which to create the specified methods.

If using this method in the guise of C<import()> then this
field will cause an error so it is only of use when calling
as C<create_class()>.

=item *

B<version> is also optional and specifies the value to give
C<$VERSION> in the class. It's generally not recommended
unless you're combining with the I<class> option. A
C<ExtUtils::MakeMaker> / C<CPAN> compliant version
specification is much better.

=back

In addition to creating any of the methods it also creates a
C<new()> method that can instantiate (or clone) objects.

=head1 SUBCLASSING

In the rest of the documentation I've often lied in order to
get some of the ideas across more easily. The thing is, this
module's very flexible. You can get markedly different
behaviour from simply subclassing it and overriding some
methods.

=head2 create_method

Given a parser coderef, returns a coderef that is suitable
to be a method.

The default action is to call C<on_fail()> in the event of a
non-parse, but you can make it do whatever you want.

=head2 on_fail

This is called in the event of a non-parse (unless you've
overridden C<create_method()> to do something else.

The single argument is the input string. The default action
is to call C<croak()>. Above, where I've said parsers or
methods throw errors, this is the method that is doing the
error throwing.

You could conceivably override this method to, say, return
C<undef>.

=head1 USING BUILDER OBJECTS aka USERS USING BUILDER

The methods listed in the L<METHODS> section are all you
generally need when creating your own class. Sometimes
you may not want a full blown class to parse something just
for this one program. Some methods are provided to make that
task easier.

=head2 new

The basic constructor. It takes no arguments, merely returns
a new C<DateTime::Format::Builder> object.

    my $parser = DateTime::Format::Builder->new();

If called as a method on an object (rather than as a class
method), then it clones the object.

    my $clone = $parser->new();

=head2 clone

Provided for those who prefer an explicit C<clone()> method
rather than using C<new()> as an object method.

    my $clone_of_clone = $clone->clone();

=head2 parser

Given either a single or multiple parser specification, sets
the object to have a parser based on that specification.

    $parser->parser(
	regex  => qr/^ (\d{4}) (\d\d) (\d\d) $/x;
	params => [qw( year    month  day    )],
    );

The arguments given to C<parser()> are handed directly to
C<create_parser()>. The resultant parser is passed to
C<set_parser()>.

If called as an object method, it returns the object.

If called as a class method, it creates a new object, sets
its parser and returns that object.

=head2 set_parser

Sets the parser of the object to the given parser.

   $parser->set_parser( $coderef );

Note: this method does not take specifications. It also does
not take anything except coderefs. Luckily, coderefs are
what most of the other methods produce.

The method return value is the object itself.

=head2 get_parser

Returns the parser the object is using.

   my $code = $parser->get_parser();

=head2 parse_datetime

Given a string, it calls the parser and returns the
C<DateTime> object that results.

   my $dt = $parser->parse_datetime( "1979 07 16" );

The return value, if not a C<DateTime> object, is whatever
the parser wants to return. Generally this means that if the
parse failed an error will be thrown.

=head2 format_datetime

If you call this function, it will throw an errror.

=head1 LONGER EXAMPLES

Some longer examples are provided in the distribution. These
implement some of the common parsing DateTime modules using
Builder. Each of them are, or were, drop in replacements for
the modules at the time of writing them.

=head1 THANKS

Dave Rolsky (DROLSKY) for kickstarting the DateTime project,
writing L<DateTime::Format::ICal> and
L<DateTime::Format::MySQL>, and some much needed review.

Joshua Hoblitt (JHOBLITT) for the concept, some of the API,
impetus for writing the multilength code (both one length with
multiple parsers and single parser with multiple lengths),
blame for the Regex custom constructor code,
spotting a bug in Dispatch,
and more much needed review.

Kellan Elliott-McCrea (KELLAN) for even more review,
suggestions, L<DateTime::Format::W3CDTF> and the encouragement to
rewrite these docs almost 100%!

Claus FE<auml>rber (CFAERBER) for having me get around to
fixing the auto-constructor writing, providing the
'args'/'self' patch, and suggesting the multi-callbacks.

Rick Measham (RICKM) for L<DateTime::Format::Strptime>
which Builder now supports.

Matthew McGillis for pointing out that C<on_fail> overriding
should be simpler.

Simon Cozens (SIMON) for saying it was cool.

=head1 SUPPORT

Support for this module is provided via the datetime@perl.org email
list. See http://lists.perl.org/ for more details.

Alternatively, log them via the CPAN RT system via the web or email:

    http://rt.cpan.org/NoAuth/ReportBug.html?Queue=DateTime%3A%3AFormat%3A%3ABuilder
    bug-datetime-format-builder@rt.cpan.org

This makes it much easier for me to track things and thus means
your problem is less likely to be neglected.

=head1 SEE ALSO

C<datetime@perl.org> mailing list.

http://datetime.perl.org/

L<perl>, L<DateTime>, L<DateTime::Format::Builder::Tutorial>,
L<DateTime::Format::Builder::Parser>

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
