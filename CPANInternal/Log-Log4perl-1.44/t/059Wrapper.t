#!/usr/local/bin/perl -w

BEGIN { 
    if($ENV{INTERNAL_DEBUG}) {
        require Log::Log4perl::InternalDebug;
        Log::Log4perl::InternalDebug->enable();
    }
}

use strict;
use Log::Log4perl qw(:easy);

############################################
# Tests for Log4perl used by a wrapper class
# Mike Schilli, 2009 (m@perlmeister.com)
###########################################
use warnings;
use strict;

use Test::More;

BEGIN { plan tests => 12 }

###########################################
package L4p::RelayWrapper;
###########################################
no strict qw(refs);
sub get_logger;
Log::Log4perl->wrapper_register(__PACKAGE__);

*get_logger = sub {

    my @args = @_;

    local $Log::Log4perl::caller_depth = 
          $Log::Log4perl::caller_depth + 1;

    if(defined $args[0] and $args[0] eq __PACKAGE__) {
         my $pkg = __PACKAGE__;
         $args[0] =~ s/$pkg/Log::Log4perl/g;
    }
    Log::Log4perl::get_logger( @args );
};

###########################################
package L4p::InheritWrapper;
###########################################
our @ISA = qw(Log::Log4perl);
Log::Log4perl->wrapper_register(__PACKAGE__);

###########################################
package main;
###########################################

use Log::Log4perl qw(get_logger);

my $pkg    = "Wobble::Cobble";
my $pkgcat = "Wobble.Cobble";

my $logger;

$logger = get_logger();
is $logger->{category}, "main", "imported get_logger()";

$logger = get_logger( $pkg );
is $logger->{category}, $pkgcat, "imported get_logger($pkg)";

for my $class (qw(Log::Log4perl
                  L4p::RelayWrapper 
                  L4p::InheritWrapper)) {

    no strict 'refs';

    my $func = "$class\::get_logger";

    if($class !~ /Inherit/) {
          # wrap::()
        $logger = $func->();
        is $logger->{category}, "main", "$class\::()";

        $logger = $func->( $pkg );
        is $logger->{category}, $pkgcat, "$class\::($pkg)";
    }

      # wrap->()
    $logger = $class->get_logger();
    is $logger->{category}, "main", "$class->()";

    $logger = $class->get_logger($pkg);
    is $logger->{category}, $pkgcat, "$class->($pkg)";
}

# use Data::Dumper;
# print Dumper($logger;
