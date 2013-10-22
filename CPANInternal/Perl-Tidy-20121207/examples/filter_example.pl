#!/usr/bin/perl -w
use Perl::Tidy;

# Illustrate use of prefilter and postfilter parameters to perltidy.  
# This example program uses a prefilter it to convert the 'method'
# keyword to 'sub', and a postfilter to convert back, so that perltidy will
# work for Method::Signature::Simple code.  
# NOTE: This program illustrates the use of filters but has not been
# extensively tested.  

# usage:
#   perl filter_example.pl filter_example.in
#
# How it works:
# 1. First the prefilter changes lines beginning with 'method foo' to 'sub
# METHOD_foo'
# 2. Then perltidy formats the code
# 3. Then the postfilter changes 'sub METHOD_' to 'method ' everywhere.
# (This assumes that there are no methods named METHOD_*, and that the keyword
# method always begins a line in the input file).  
#
# Debugging hints: 
# 1. Try commenting out the postfilter and running with 
# the --notidy option to see what the prefilter alone is doing.
# 2. Then run with both pre- and post ters with --notidy to be sure
# that the postfilter properly undoes the prefilter.

my $arg_string = undef;
my $err=Perl::Tidy::perltidy(
    argv => $arg_string,
    prefilter =>
      sub { $_ = $_[0]; s/^\s*method\s+(\w.*)/sub METHOD_$1/gm; return $_ },
    postfilter =>
      sub { $_ = $_[0]; s/sub\s+METHOD_/method /gm; return $_ }
);
if ($err) {
    die "Error calling perltidy\n";
}
__END__

# Try running on the following code (file filter_example.in):

use Method::Signatures::Simple;

 method foo { $self->bar }

       # with signature
    method foo($bar, %opts) { $self->bar(reverse $bar) if $opts{rev};
    }

    # attributes
    method foo : lvalue { $self->{foo} 
}

 # change invocant name
    method 
foo ($class: $bar) { $class->bar($bar) }
