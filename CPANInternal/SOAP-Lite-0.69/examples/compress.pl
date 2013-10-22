#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use SOAP::Lite;

$s = SOAP::Lite
  -> uri('http://localhost/My/Parameters')
  -> proxy('http://localhost/soap')
;

use Benchmark;

$s->echo; # ignore the first call

Benchmark::cmpthese(10, {
  'nocompress' => sub { $s->transport->options({}); $s->echo(1 x 100000) },
  'compress' => sub { $s->transport->options({compress_threshold => 10000}); $s->echo(1 x 100000) },
})
