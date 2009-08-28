#!/usr/bin/perl -w
use strict;
use warnings;
use XML::LibXML;
use Test;
BEGIN { 
  plan tests => 7;
};


# tests for bug #24953: External entities not expanded in included file (XInclude)

my $parser = XML::LibXML->new;
my $file = 'test/xinclude/test.xml';
{
  $parser->expand_xinclude(0);
  $parser->expand_entities(1);
  ok($parser->parse_file($file)->toString() !~  /IT WORKS/);
}
{
  $parser->expand_xinclude(1);
  $parser->expand_entities(0);
  ok($parser->parse_file($file)->toString() !~  /IT WORKS/);
}
{
  $parser->expand_xinclude(1);
  $parser->expand_entities(1);
  ok($parser->parse_file($file)->toString() =~  /IT WORKS/);
}
{
  $parser->expand_xinclude(0);
  my $doc = $parser->parse_file($file);
  ok( $doc->process_xinclude({expand_entities=>0}) );
  ok( $doc->toString()!~/IT WORKS/ );
}
{
  my $doc = $parser->parse_file($file);
  ok( $doc->process_xinclude({expand_entities=>1}) );
  ok( $doc->toString()=~/IT WORKS/ );
}
