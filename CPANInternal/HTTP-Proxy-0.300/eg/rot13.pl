#!/usr/bin/perl -w
use HTTP::Proxy qw( :log );
use HTTP::Proxy::BodyFilter::tags;
use HTTP::Proxy::BodyFilter::simple;
use HTTP::Proxy::BodyFilter::htmltext;
use strict;

my $proxy = HTTP::Proxy->new(@ARGV);

my %noaccent = (
    Agrave => 'A', Aacute => 'A', Acirc  => 'A',  Atilde => 'A',
    Auml   => 'A', Aring  => 'A', AElig  => 'AE', Ccedil => 'C',
    Egrave => 'E', Eacute => 'E', Ecirc  => 'E',  Euml   => 'E',
    Igrave => 'I', Iacute => 'I', Icirc  => 'I',  Iuml   => 'I',
    Ntilde => 'N', Ograve => 'O', Oacute => 'O',  Ocirc  => 'O',
    Otile  => 'O', Ouml   => 'O', Oslash => 'O',  Ugrave => 'U',
    Uacute => 'U', Ucirc  => 'U', Uuml   => 'U',  Yacute => 'Y',
    agrave => 'a', aacute => 'a', acirc  => 'a',  atilde => 'a',
    auml   => 'a', aring  => 'a', aelig  => 'ae', ccedil => 'c',
    egrave => 'e', eacute => 'e', ecirc  => 'e',  euml   => 'e',
    igrave => 'i', iacute => 'i', icirc  => 'i',  iuml   => 'i',
    ntilde => 'n', ograve => 'o', oacute => 'o',  ocirc  => 'o',
    otile  => 'o', ouml   => 'o', oslash => 'o',  ugrave => 'u',
    uacute => 'u', ucirc  => 'u', uuml   => 'u',  yacute => 'y',
    'yuml' => 'y', 'Æ'    => 'AE', 'æ'   => 'ae',
);

my $re = join '|', sort keys %noaccent;

$proxy->push_filter(
    mime     => 'text/html',
    response => HTTP::Proxy::BodyFilter::tags->new,      # protect tags
    response => HTTP::Proxy::BodyFilter::simple->new(    # remove accents
        sub { ${ $_[1] } =~ s/&($re);/$noaccent{$1}/go; }
    ),
    response => HTTP::Proxy::BodyFilter::htmltext->new(    # rot13
        sub {
            tr{ÀÁÂÃÄÅÇÈÉÊËÌÍÎÏÑÒÓÔÕÖØÙÚÛÜİàáâãäåçèéêëìíîïñòóôõöøùúûüıÿ}
              {AAAAAACEEEEIIIINOOOOOOUUUUYaaaaaaceeeeiiiinoooooouuuuyy};
            tr/a-zA-z/n-za-mN-ZA-M/;
        }
    )
);

$proxy->start;

