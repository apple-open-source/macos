#!/usr/bin/perl -w

use strict;

use Test::More;

use Params::Validate qw(validate validate_with);

my @testset;

# Generate test cases ...
BEGIN
{
    my @lower_case_args = ( foo => 1 );
    my @upper_case_args = ( FOO => 1 );
    my @mixed_case_args = ( FoO => 1 );

    my %lower_case_spec = ( foo => 1 );
    my %upper_case_spec = ( FOO => 1 );
    my %mixed_case_spec = ( FoO => 1 );

    my %arglist = ( lower => \@lower_case_args,
                    upper => \@upper_case_args,
                    mixed => \@mixed_case_args
                  );

    my %speclist = ( lower => \%lower_case_spec,
                     upper => \%upper_case_spec,
                     mixed => \%mixed_case_spec
                   );

    # XXX - make subs such that user gets to see the error message
    # when a test fails
    my $ok_sub  =
        sub { if ( $@ )
              {
                  print STDERR $@;
              }
              !$@; };

    my $nok_sub =
        sub { my $ok = ( $@ =~ /not listed in the validation options/ );
              unless ($ok)
              {
                  print STDERR $@;
              }
              $ok; };

    # generate testcases on the fly (I'm too lazy)
    for my $ignore_case ( qw( 0 1 ) )
    {
        for my $args (keys %arglist)
        {
            for my $spec (keys %speclist)
            {
                push @testset, { params => $arglist{ $args },
                                 spec   => $speclist{ $spec },
                                 expect =>
                                 ( $ignore_case
                                   ? $ok_sub
                                   : $args eq $spec
                                   ? $ok_sub
                                   : $nok_sub
                                 ),
                                 ignore_case => $ignore_case
                               };
            }
        }
    }
}

plan tests => (scalar @testset) * 2;

{
    # XXX - "called" will be all messed up, but what the heck
    foreach my $case (@testset)
    {
        my %args =
            eval { validate_with( params      => $case->{params},
                                  spec        => $case->{spec},
                                  ignore_case => $case->{ignore_case}
                                ) };

        ok( $case->{expect}->(%args) );
    }

    # XXX - make sure that it works from validation_options() as well
    foreach my $case (@testset)
    {
        Params::Validate::validation_options
            ( ignore_case => $case->{ignore_case} );

        my %args = eval { my @args = @{ $case->{params} };
                          validate( @args, $case->{spec} ) };

        ok( $case->{expect}->(%args) );
    }
}

