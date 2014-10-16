#!/usr/bin/perl -w

# script kindly offered by glb (Eric Cassagnard)

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
  'yuml' => 'y', 'Æ' => 'AE', 'æ' => 'ae',
);
my $re = join '|', sort keys %noaccent;

my %sounds = (
  an => 'un', An  => 'Un',  au  => 'oo',  Au => 'Oo', a    => 'e',
  A  => 'E',  ew  => 'oo',  e   => 'e-a', e  => 'i',  E    => 'I',
  f  => 'ff', ir  => 'ur',  ow  => 'oo',  o  => 'oo', O    => 'Oo',
  o  => 'u',  the => 'zee', The => 'Zee', th => 't',  tion => 'shun',
  u  => 'oo', U   => 'Oo',  v   => 'f',   V  => 'F',  w    => 'v',
  W  => 'V' );
my $sc = join '|', sort keys %sounds;

$proxy->push_filter(
  mime     => 'text/html',
  response => HTTP::Proxy::BodyFilter::tags->new,
  response => HTTP::Proxy::BodyFilter::simple->new(
    sub { ${ $_[ 1 ] } =~ s/&($re);/$noaccent{$1}/go; }
  ),
  response => HTTP::Proxy::BodyFilter::htmltext->new(
    sub {
      tr{ÀÁÂÃÄÅÇÈÉÊËÌÍÎÏÑÒÓÔÕÖØÙÚÛÜİàáâãäåçèéêëìíîïñòóôõöøùúûüıÿ}
        {AAAAAACEEEEIIIINOOOOOOUUUUYaaaaaaceeeeiiiinoooooouuuuyy};
      s/($sc)/$sounds{$1}/go;
      s/([?!]+)/$1 Bork bork bork&nbsp;!/go ;
      s/(\.+)(\s|$)/$1 Bork bork bork&nbsp;! /go ;
       }
  )
);

$proxy->start;

