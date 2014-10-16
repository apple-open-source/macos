#!/usr/bin/perl -w
use HTTP::Proxy;
use HTTP::Proxy::HeaderFilter::simple;
use Fcntl ':flock';
use strict;

# this is a tracker proxy that stores Referer, URL, CODE
# and output them to STDOUT or the given file
#
# Example output:
#
# NULL http://www.perl.org/ 200
# http://www.perl.org/ http://learn.perl.org/ 200
#
my $file = shift || '-';
open OUT, ">> $file" or die "Can't open $file: $!";

my $proxy = HTTP::Proxy->new( @ARGV ); # pass the args you want
$proxy->push_filter(
    response => HTTP::Proxy::HeaderFilter::simple->new(
        sub {
            my ( $self, $headers, $message ) = @_;

            flock( OUT, LOCK_EX );
            print OUT join( " ",
                  $message->request->headers->header( 'Referer' ) || 'NULL',
                  $message->request->uri,
                  $message->code ), $/;
            flock( OUT, LOCK_UN );
        }
    )
);
$proxy->start;

END { close OUT; }

