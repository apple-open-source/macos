#!/usr/bin/perl -w
use Test::More tests => 16;
BEGIN {use_ok('Pod::WSDL::Writer')}
use strict;
use warnings;

# ****************************************************************
# test constructor()

my $wr = new Pod::WSDL::Writer(pretty => 1, withDocumentation => 1);

ok($wr->{_pretty} == 1, 'Constructor: Read argument pretty correctly.');
ok($wr->{_withDocumentation} == 1, 'Constructor: Read argument withDocumentation correctly.');
ok($wr->{_indent} == 1, 'Constructor: Initialized indentation correctly.');
ok($wr->{_lastTag} eq '', 'Constructor: Initialized last tag correctly.');
ok((ref $wr->{_faultMessageWritten} eq 'HASH'), 'Constructor: Initialized "faultMessageWritten" correctly.');
ok($wr->output eq '<?xml version="1.0" encoding="UTF-8"?>' . "\n", 'Constructor: Initialized XML::Writer correctly.');

# ****************************************************************
# test wrNewLine()
$wr->startTag('bla');
$wr->wrNewLine();
$wr->endTag('bla');
my $expectedOutput =<<EOU;
<?xml version="1.0" encoding="UTF-8"?>
<bla>
</bla>
EOU
ok($wr->output . "\n" eq $expectedOutput, 'wrNewLine works.');

# ****************************************************************
# test wrElem()
$wr = new Pod::WSDL::Writer();
$wr->wrElem('empty', 'foo', bar => 1, bloerch => 'ggg');
$expectedOutput =<<EOU;
<?xml version="1.0" encoding="UTF-8"?>
<foo bar="1" bloerch="ggg" />
EOU
ok($wr->output . "\n" eq $expectedOutput, 'Writing empty elements works.');

$wr = new Pod::WSDL::Writer();
$wr->wrElem('start', 'foo', bar => 1, bloerch => 'ggg');
$wr->wrElem('end', 'foo', bar => 1, bloerch => 'ggg');
$expectedOutput =<<EOU;
<?xml version="1.0" encoding="UTF-8"?>
<foo bar="1" bloerch="ggg"></foo>
EOU
ok($wr->output . "\n" eq $expectedOutput, 'Writing non empty elements works.');

# ****************************************************************
# test wrDoc()
$wr = new Pod::WSDL::Writer(withDocumentation => 1);
$wr->wrElem('start', 'foo', bar => 1, bloerch => 'ggg');
$wr->wrDoc('This is my documentation.');
$wr->wrElem('end', 'foo', bar => 1, bloerch => 'ggg');
$expectedOutput =<<EOU;
<?xml version="1.0" encoding="UTF-8"?>
<foo bar="1" bloerch="ggg"><wsdl:documentation>This is my documentation.</wsdl:documentation></foo>
EOU
ok($wr->output . "\n" eq $expectedOutput, 'wrDoc works.');

$wr = new Pod::WSDL::Writer(withDocumentation => 0);
$wr->wrElem('start', 'foo', bar => 1, bloerch => 'ggg');
$wr->wrDoc('This is my documentation.');
$wr->wrElem('end', 'foo', bar => 1, bloerch => 'ggg');
$expectedOutput =<<EOU;
<?xml version="1.0" encoding="UTF-8"?>
<foo bar="1" bloerch="ggg"></foo>
EOU
ok($wr->output . "\n" eq $expectedOutput, 'wrDoc writes no documentation when object not initialized with withDocumentation.');

# ****************************************************************
# test withDocumentation()
$wr = new Pod::WSDL::Writer(withDocumentation => 1);
$wr->withDocumentation(0);
$wr->wrElem('start', 'foo', bar => 1, bloerch => 'ggg');
$wr->wrDoc('This is my documentation.');
$wr->wrElem('end', 'foo', bar => 1, bloerch => 'ggg');
$expectedOutput =<<EOU;
<?xml version="1.0" encoding="UTF-8"?>
<foo bar="1" bloerch="ggg"></foo>
EOU
ok($wr->output . "\n" eq $expectedOutput, 'wrDoc works.');

$wr = new Pod::WSDL::Writer(withDocumentation => 0);
$wr->withDocumentation(1);
$wr->wrElem('start', 'foo', bar => 1, bloerch => 'ggg');
$wr->wrDoc('This is my documentation.');
$wr->wrElem('end', 'foo', bar => 1, bloerch => 'ggg');
$expectedOutput =<<EOU;
<?xml version="1.0" encoding="UTF-8"?>
<foo bar="1" bloerch="ggg"><wsdl:documentation>This is my documentation.</wsdl:documentation></foo>
EOU
ok($wr->output . "\n" eq $expectedOutput, 'wrDoc writes no documentation when object not initialized with withDocumentation.');

# ****************************************************************
# test registerWrittenFaultMessage() and faultMessageWritten()
$wr->registerWrittenFaultMessage('bar');
ok($wr->faultMessageWritten('bar'), 'Registering written fault messages seems to work.');

# ****************************************************************
# test AUTOLOADING
eval {$wr->bla;};
ok($@ =~ /Can't locate object method "bla" via package "XML::Writer"/, 'AUTOLOADER using XML::Writer correctly.')
