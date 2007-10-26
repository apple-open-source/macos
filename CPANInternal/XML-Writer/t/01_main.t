#!/usr/bin/perl -w
########################################################################
# test.pl - test script for XML::Writer module.
# Copyright (c) 1999 by Megginson Technologies.
# Copyright (c) 2004, 2005 by Joseph Walton <joe@kafsemo.org>.
# No warranty.  Commercial and non-commercial use freely permitted.
#
# $Id: 01_main.t,v 1.22 2005/06/30 21:57:52 josephw Exp $
########################################################################

# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl 01_main.t'

use strict;

use Test::More(tests => 207);


# Catch warnings
my $warning;

$SIG{__WARN__} = sub {
	($warning) = @_ unless ($warning);
};

sub wasNoWarning($)
{
	my ($reason) = @_;

	if (!ok(!$warning, $reason)) {
		diag($warning);
	}
}

# Constants for Unicode support
my $unicodeSkipMessage = 'Unicode only supported with Perl >= 5.8.1';

sub isUnicodeSupported()
{
	return $] >= 5.008001;
}

require XML::Writer;

wasNoWarning('Loading XML::Writer should not result in warnings');

use IO::File;

# The XML::Writer that will be used
my $w;

my $outputFile = IO::File->new_tmpfile or die "Unable to create temporary file: $!";

# Fetch the current contents of the scratch file as a scalar
sub getBufStr()
{
	local($/);
	binmode($outputFile, ':bytes') if isUnicodeSupported();
	$outputFile->seek(0, 0);
	return <$outputFile>;
}

# Set up the environment to run a test.
sub initEnv(@)
{
	my (%args) = @_;

	# Reset the scratch file
	$outputFile->seek(0, 0);
	$outputFile->truncate(0);
	binmode($outputFile, ':raw');

	# Overwrite OUTPUT so it goes to the scratch file
	$args{'OUTPUT'} = $outputFile;

	# Set NAMESPACES, unless it's present
	$args{'NAMESPACES'} = 1 unless(defined($args{'NAMESPACES'}));

	undef($warning);
	$w = new XML::Writer(%args) || die "Cannot create XML writer";
}

#
# Check the results in the temporary output file.
#
# $expected - the exact output expected
#
sub checkResult($$)
{
	my ($expected, $explanation) = (@_);

	my $actual = getBufStr();

	if ($expected eq $actual) {
		ok(1, $explanation);
	} else {
		my @e = split(/\n/, $expected);
		my @a = split(/\n/, $actual);

		if (@e + @a == 2) {
			is(getBufStr(), $expected, $explanation);
		} else {
			if (eval {require Algorithm::Diff;}) {
				fail($explanation);

				Algorithm::Diff::traverse_sequences( \@e, \@a, {
					MATCH => sub { diag(" $e[$_[0]]\n"); },
					DISCARD_A => sub { diag("-$e[$_[0]]\n"); },
					DISCARD_B => sub { diag("+$a[$_[1]]\n"); }
				});
			} else {
				fail($explanation);
				diag("         got: '$actual'\n");
				diag("    expected: '$expected'\n");
			}
		}
	}

	wasNoWarning('(no warnings)');
}

#
# Expect an error of some sort, and check that the message matches.
#
# $pattern - a regular expression that must match the error message
# $value - the return value from an eval{} block
#
sub expectError($$) {
	my ($pattern, $value) = (@_);
	if (!ok((!defined($value) and ($@ =~ $pattern)), "Error expected: $pattern"))
	{
		diag('Actual error:');
		if ($@) {
			diag($@);
		} else {
			diag('(no error)');
			diag(getBufStr());
		}
	}
}

# Empty element tag.
TEST: {
	initEnv();
	$w->emptyTag("foo");
	$w->end();
	checkResult("<foo />\n", 'An empty element tag');
};

# Empty element tag with XML decl.
TEST: {
	initEnv();
	$w->xmlDecl();
	$w->emptyTag("foo");
	$w->end();
	checkResult(<<"EOS", 'Empty element tag with XML declaration');
<?xml version="1.0"?>
<foo />
EOS
};

# A document with a public and system identifier set
TEST: {
	initEnv();
	$w->doctype('html', "-//W3C//DTD XHTML 1.1//EN",
						"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd");
	$w->emptyTag('html');
	$w->end();
	checkResult(<<"EOS", 'A document with a public and system identifier');
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">
<html />
EOS
};

# A document with a public and system identifier set, using startTag
TEST: {
	initEnv();
	$w->doctype('html', "-//W3C//DTD XHTML 1.1//EN",
						"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd");
	$w->startTag('html');
	$w->endTag('html');
	$w->end();
	checkResult(<<"EOS", 'A document with a public and system identifier');
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">
<html></html>
EOS
};

# A document with a only a public identifier
TEST: {
	initEnv();
	expectError("A DOCTYPE declaration with a public ID must also have a system ID", eval {
		$w->doctype('html', "-//W3C//DTD XHTML 1.1//EN");
	});
};

# A document with only a system identifier set
TEST: {
	initEnv();
	$w->doctype('html', undef, "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd");
	$w->emptyTag('html');
	$w->end();
	checkResult(<<"EOS", 'A document with just a system identifier');
<!DOCTYPE html SYSTEM "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">
<html />
EOS
};

# Empty element tag with standalone set
TEST: {
	initEnv();
	$w->xmlDecl(undef, 'yes');
	$w->emptyTag("foo");
	$w->end();
	checkResult(<<"EOS", 'A document with "standalone" declared');
<?xml version="1.0" standalone="yes"?>
<foo />
EOS
};

# Empty element tag with standalone explicitly set to 'no'
TEST: {
	initEnv();
	$w->xmlDecl(undef, 'no');
	$w->emptyTag("foo");
	$w->end();
	checkResult(<<"EOS", "A document with 'standalone' declared as 'no'");
<?xml version="1.0" standalone="no"?>
<foo />
EOS
};

# xmlDecl with encoding set
TEST: {
	initEnv();
	$w->xmlDecl('ISO-8859-1');
	$w->emptyTag("foo");
	$w->end();
	checkResult(<<"EOS", 'A document with a declared encoding');
<?xml version="1.0" encoding="ISO-8859-1"?>
<foo />
EOS
};

# Start/end tag.
TEST: {
	initEnv();
	$w->startTag("foo");
	$w->endTag("foo");
	$w->end();
	checkResult("<foo></foo>\n", 'A separate start and end tag');
};

# Attributes
TEST: {
	initEnv();
	$w->emptyTag("foo", "x" => "1>2");
	$w->end();
	checkResult("<foo x=\"1&gt;2\" />\n", 'Simple attributes');
};

# Character data
TEST: {
	initEnv();
	$w->startTag("foo");
	$w->characters("<tag>&amp;</tag>");
	$w->endTag("foo");
	$w->end();
	checkResult("<foo>&lt;tag&gt;&amp;amp;&lt;/tag&gt;</foo>\n", 'Escaped character data');
};

# Comment outside document element
TEST: {
	initEnv();
	$w->comment("comment");
	$w->emptyTag("foo");
	$w->end();
	checkResult("<!-- comment -->\n<foo />\n", 'A comment outside the document element');
};

# Processing instruction without data (outside document element)
TEST: {
	initEnv();
	$w->pi("pi");
	$w->emptyTag("foo");
	$w->end();
	checkResult("<?pi?>\n<foo />\n", 'A data-less processing instruction');
};

# Processing instruction with data (outside document element)
TEST: {
	initEnv();
	$w->pi("pi", "data");
	$w->emptyTag("foo");
	$w->end();
	checkResult("<?pi data?>\n<foo />\n", 'A processing instruction with data');
};

# Comment inside document element
TEST: {
	initEnv();
	$w->startTag("foo");
	$w->comment("comment");
	$w->endTag("foo");
	$w->end();
	checkResult("<foo><!-- comment --></foo>\n", 'A comment inside an element');
};

# Processing instruction inside document element
TEST: {
	initEnv();
	$w->startTag("foo");
	$w->pi("pi");
	$w->endTag("foo");
	$w->end();
	checkResult("<foo><?pi?></foo>\n", 'A processing instruction inside an element');
};

# WFE for mismatched tags
TEST: {
	initEnv();
	$w->startTag("foo");
	expectError("Attempt to end element \"foo\" with \"bar\" tag", eval {
		$w->endTag("bar");
	});
};

# WFE for unclosed elements
TEST: {
	initEnv();
	$w->startTag("foo");
	$w->startTag("foo");
	$w->endTag("foo");
	expectError("Document ended with unmatched start tag\\(s\\)", eval {
		$w->end();
	});
};

# WFE for no document element
TEST: {
	initEnv();
	$w->xmlDecl();
	expectError("Document cannot end without a document element", eval {
		$w->end();
	});
};

# WFE for multiple document elements (non-empty)
TEST: {
	initEnv();
	$w->startTag('foo');
	$w->endTag('foo');
	expectError("Attempt to insert start tag after close of", eval {
		$w->startTag('foo');
	});
};

# WFE for multiple document elements (empty)
TEST: {
	initEnv();
	$w->emptyTag('foo');
	expectError("Attempt to insert empty tag after close of", eval {
		$w->emptyTag('foo');
	});
};

# DOCTYPE mismatch with empty tag
TEST: {
	initEnv();
	$w->doctype('foo');
	expectError("Document element is \"bar\", but DOCTYPE is \"foo\"", eval {
		$w->emptyTag('bar');
	});
};

# DOCTYPE mismatch with start tag
TEST: {
	initEnv();
	$w->doctype('foo');
	expectError("Document element is \"bar\", but DOCTYPE is \"foo\"", eval {
		$w->startTag('bar');
	});
};

# DOCTYPE declarations
TEST: {
	initEnv();
	$w->doctype('foo');
	expectError("Attempt to insert second DOCTYPE", eval {
		$w->doctype('bar');
	});
};

# Misplaced DOCTYPE declaration
TEST: {
	initEnv();
	$w->startTag('foo');
	expectError("The DOCTYPE declaration must come before", eval {
		$w->doctype('foo');
	});
};

# Multiple XML declarations
TEST: {
	initEnv();
	$w->xmlDecl();
	expectError("The XML declaration is not the first thing", eval {
		$w->xmlDecl();
	});
};

# Misplaced XML declaration
TEST: {
	initEnv();
	$w->comment();
	expectError("The XML declaration is not the first thing", eval {
		$w->xmlDecl();
	});
};

# Implied end-tag name.
TEST: {
	initEnv();
	$w->startTag('foo');
	$w->endTag();
	$w->end();
	checkResult("<foo></foo>\n", 'A tag ended using an implied tag name');
};

# in_element query
TEST: {
	initEnv();
	$w->startTag('foo');
	$w->startTag('bar');
	ok($w->in_element('bar'), 'in_element should identify the current element');
};

# within_element query
TEST: {
	initEnv();
	$w->startTag('foo');
	$w->startTag('bar');
	ok($w->within_element('foo') && $w->within_element('bar'),
		'within_element should know about all elements above us');
};

# current_element query
TEST: {
	initEnv();
	$w->startTag('foo');
	$w->startTag('bar');
	is($w->current_element(), 'bar', 'current_element should identify the element we are in');
};

# ancestor query
TEST: {
	initEnv();
	$w->startTag('foo');
	$w->startTag('bar');
	ok($w->ancestor(0) eq 'bar' && $w->ancestor(1) eq 'foo',
		'ancestor() should match the startTag calls that have been made');
};

# Basic namespace processing with empty element
TEST: {
	initEnv();
	my $ns = 'http://www.foo.com/';
	$w->addPrefix($ns, 'foo');
	$w->emptyTag([$ns, 'doc']);
	$w->end();
	checkResult("<foo:doc xmlns:foo=\"$ns\" />\n", 'Basic namespace processing');
};

# Basic namespace processing with start/end tags
TEST: {
	initEnv();
	my $ns = 'http://www.foo.com/';
	$w->addPrefix($ns, 'foo');
	$w->startTag([$ns, 'doc']);
	$w->endTag([$ns, 'doc']);
	$w->end();
	checkResult("<foo:doc xmlns:foo=\"$ns\"></foo:doc>\n", 'Basic namespace processing');
};

# Basic namespace processing with generated prefix
TEST: {
	initEnv();
	my $ns = 'http://www.foo.com/';
	$w->startTag([$ns, 'doc']);
	$w->endTag([$ns, 'doc']);
	$w->end();
	checkResult("<__NS1:doc xmlns:__NS1=\"$ns\"></__NS1:doc>\n",
		'Basic namespace processing with a generated prefix');
};

# Basic namespace processing with attributes and empty tag.
TEST: {
	initEnv();
	my $ns = 'http://www.foo.com/';
	$w->addPrefix($ns, 'foo');
	$w->emptyTag([$ns, 'doc'], [$ns, 'id'] => 'x');
	$w->end();
	checkResult("<foo:doc foo:id=\"x\" xmlns:foo=\"$ns\" />\n",
		'A namespaced element with a namespaced attribute');
};

# Same as above, but with default namespace.
TEST: {
	initEnv();
	my $ns = 'http://www.foo.com/';
	$w->addPrefix($ns, '');
	$w->emptyTag([$ns, 'doc'], [$ns, 'id'] => 'x');
	$w->end();
	checkResult("<doc __NS1:id=\"x\" xmlns=\"$ns\" xmlns:__NS1=\"$ns\" />\n",
		'Same as above, but with a default namespace');
};

# Same as above, but passing namespace prefixes through constructor
TEST: {
	my $ns = 'http://www.foo.com/';
	initEnv(PREFIX_MAP => {$ns => ''});
	$w->emptyTag([$ns, 'doc'], [$ns, 'id'] => 'x');
	$w->end();
	checkResult("<doc __NS1:id=\"x\" xmlns=\"$ns\" xmlns:__NS1=\"$ns\" />\n",
		'Same as above, but passing the prefixes through the constructor');
};

# Same as above, but passing namespace prefixes through constructor and
# then removing them programatically
TEST: {
	my $ns = 'http://www.foo.com/';
	initEnv(PREFIX_MAP => {$ns => ''});
	$w->removePrefix($ns);
	$w->emptyTag([$ns, 'doc'], [$ns, 'id'] => 'x');
	$w->end();
	checkResult("<__NS1:doc __NS1:id=\"x\" xmlns:__NS1=\"$ns\" />\n",
		'Same as above, but removing the prefix before the document starts');
};

# Verify that removePrefix works when there is no default prefix
TEST: {
	my $ns = 'http://www.foo.com/';
	initEnv(PREFIX_MAP => {$ns => 'pfx'});
	$w->removePrefix($ns);
	wasNoWarning('removePrefix should not warn when there is no default prefix');
}

# Verify that a removed namespace prefix behaves as if it were never added
TEST: {
	my $ns = 'http://www.foo.com/';
	initEnv(PREFIX_MAP => {$ns => 'pfx', 'http://www.example.com/' => ''});
	$w->removePrefix($ns);
	$w->startTag([$ns, 'x']);
	$w->emptyTag([$ns, 'y']);
	$w->endTag([$ns, 'x']);
	$w->end();
	checkResult("<__NS1:x xmlns:__NS1=\"$ns\"><__NS1:y /></__NS1:x>\n",
		'Same as above, but with a non-default namespace');
};

# Test that autogenerated prefixes avoid collision.
TEST: {
	initEnv();
	my $ns = 'http://www.foo.com/';
	$w->addPrefix('http://www.bar.com/', '__NS1');
	$w->emptyTag([$ns, 'doc']);
	$w->end();
	checkResult("<__NS2:doc xmlns:__NS2=\"$ns\" />\n",
		"Make sure that an autogenerated prefix doesn't clash");
};

# Check for proper declaration nesting with subtrees.
TEST: {
	initEnv();
	my $ns = 'http://www.foo.com/';
	$w->addPrefix($ns, 'foo');
	$w->startTag('doc');
	$w->characters("\n");
	$w->emptyTag([$ns, 'ptr1']);
	$w->characters("\n");
	$w->emptyTag([$ns, 'ptr2']);
	$w->characters("\n");
	$w->endTag('doc');
	$w->end();
	checkResult(<<"EOS", 'Check for proper declaration nesting with subtrees.');
<doc>
<foo:ptr1 xmlns:foo="$ns" />
<foo:ptr2 xmlns:foo="$ns" />
</doc>
EOS
};

# Check for proper declaration nesting with top level.
TEST: {
	initEnv();
	my $ns = 'http://www.foo.com/';
	$w->addPrefix($ns, 'foo');
	$w->startTag([$ns, 'doc']);
	$w->characters("\n");
	$w->emptyTag([$ns, 'ptr1']);
	$w->characters("\n");
	$w->emptyTag([$ns, 'ptr2']);
	$w->characters("\n");
	$w->endTag([$ns, 'doc']);
	$w->end();
	checkResult(<<"EOS", 'Check for proper declaration nesting with top level.');
<foo:doc xmlns:foo="$ns">
<foo:ptr1 />
<foo:ptr2 />
</foo:doc>
EOS
};

# Check for proper default declaration nesting with subtrees.
TEST: {
	initEnv();
	my $ns = 'http://www.foo.com/';
	$w->addPrefix($ns, '');
	$w->startTag('doc');
	$w->characters("\n");
	$w->emptyTag([$ns, 'ptr1']);
	$w->characters("\n");
	$w->emptyTag([$ns, 'ptr2']);
	$w->characters("\n");
	$w->endTag('doc');
	$w->end();
	checkResult(<<"EOS", 'Check for proper default declaration nesting with subtrees.');
<doc>
<ptr1 xmlns="$ns" />
<ptr2 xmlns="$ns" />
</doc>
EOS
};

# Check for proper default declaration nesting with top level.
TEST: {
	initEnv();
	my $ns = 'http://www.foo.com/';
	$w->addPrefix($ns, '');
	$w->startTag([$ns, 'doc']);
	$w->characters("\n");
	$w->emptyTag([$ns, 'ptr1']);
	$w->characters("\n");
	$w->emptyTag([$ns, 'ptr2']);
	$w->characters("\n");
	$w->endTag([$ns, 'doc']);
	$w->end();
	checkResult(<<"EOS", 'Check for proper default declaration nesting with top level.');
<doc xmlns="$ns">
<ptr1 />
<ptr2 />
</doc>
EOS
};

# Namespace error: attribute name beginning 'xmlns'
TEST: {
	initEnv();
	expectError("Attribute name.*begins with 'xmlns'", eval {
		$w->emptyTag('foo', 'xmlnsxxx' => 'x');
	});
};

# Namespace error: Detect an illegal colon in a PI target.
TEST: {
	initEnv();
	expectError("PI target.*contains a colon", eval {
		$w->pi('foo:foo');
	});
};

# Namespace error: Detect an illegal colon in an element name.
TEST: {
	initEnv();
	expectError("Element name.*contains a colon", eval {
		$w->emptyTag('foo:foo');
	});
};

# Namespace error: Detect an illegal colon in local part of an element name.
TEST: {
	initEnv();
	expectError("Local part of element name.*contains a colon", eval {
		my $ns = 'http://www.foo.com/';
		$w->emptyTag([$ns, 'foo:foo']);
	});
};

# Namespace error: attribute name containing ':'.
TEST: {
	initEnv();
	expectError("Attribute name.*contains ':'", eval {
		$w->emptyTag('foo', 'foo:bar' => 'x');
	});
};

# Namespace error: Detect a colon in the local part of an att name.
TEST: {
	initEnv();
	expectError("Local part of attribute name.*contains a colon.", eval {
		my $ns = "http://www.foo.com/";
		$w->emptyTag('foo', [$ns, 'foo:bar']);
	});
};

# Verify that no warning is generated when namespace prefixes are passed
# in on construction.
TEST: {
	initEnv();
	$w->emptyTag(['uri:null', 'element']);
	$w->end();

	wasNoWarning('No warnings should be generated during writing');
};

# Verify that the 'xml:' prefix is known, and that the declaration is not
# passed through.
#
TEST: {
	initEnv();
	$w->emptyTag('elem', ['http://www.w3.org/XML/1998/namespace', 'space'] => 'preserve');
	$w->end();

	if (!unlike(getBufStr(), qr/1998/, "No declaration should be generated for the 'xml:' prefix"))
	{
		diag(getBufStr());
	}
};

# This is an API-driving test; to pass, it needs an added method to force XML
# namespace declarations on outer elements that aren't necessarily
# in the namespace themselves.
TEST: {
	initEnv(PREFIX_MAP => {'uri:test', 'test'},
		FORCED_NS_DECLS => ['uri:test']
	);

	$w->startTag('doc');
	$w->emptyTag(['uri:test', 'elem']);
	$w->emptyTag(['uri:test', 'elem']);
	$w->emptyTag(['uri:test', 'elem']);
	$w->endTag('doc');
	$w->end();

	if (!unlike(getBufStr(), qr/uri:test.*uri:test/, 'An API should allow forced namespace declarations'))
	{
		diag(getBufStr());
	}
};

# Verify that a processing instruction of 'xml-stylesheet' can be added
# without causing a warning, as well as a PI that contains 'xml'
# other than at the beginning, and a PI with no data
TEST: {
	initEnv();
	$w->pi('xml-stylesheet', "type='text/xsl' href='style.xsl'");
	$w->pi('not-reserved-by-xml-spec', '');
	$w->pi('pi-with-no-data');

	$w->emptyTag('x');

	$w->end();

	wasNoWarning('The test processing instructions should not cause warnings');
};

# Verify that a still-reserved processing instruction generates 
# a warning.
TEST: {
	initEnv();
	$w->pi('xml-reserves-this-name');

	$w->emptyTag('x');
	$w->end();

	ok($warning =~ "^Processing instruction target begins with 'xml'",
		"Reserved processing instruction names should cause warnings");
};

# Processing instruction data may not contain '?>'
TEST: {
	initEnv();
	expectError("Processing instruction may not contain", eval {
		$w->pi('test', 'This string is bad?>');
	});
};
	
# A processing instruction name may not contain '?>'
TEST: {
	initEnv();
	expectError("Processing instruction may not contain", eval {
		$w->pi('bad-processing-instruction-bad?>');
	});
};

# A processing instruction name can't contain spaces
TEST: {
	initEnv();
	expectError("", eval {
		$w->pi('processing instruction');
	});
};

# Verify that dataMode can be turned on and off for specific elements
TEST: {
	initEnv(
		DATA_MODE => 1,
		DATA_INDENT => 1
	);

	ok($w->getDataMode(), 'Should be in data mode');
	$w->startTag('doc');
	$w->dataElement('data', 'This is data');
	$w->dataElement('empty', '');
	$w->emptyTag('empty');
	$w->startTag('mixed');
	$w->setDataMode(0);
	$w->characters('This is ');
	$w->emptyTag('mixed');
	ok(!$w->getDataMode(), 'Should be in mixed mode');
	$w->characters(' ');
	$w->startTag('x');
	$w->characters('content');
	$w->endTag('x');
	$w->characters('.');
	$w->setDataMode(1);
	$w->setDataIndent(5);
	$w->endTag('mixed');
	is($w->getDataIndent(), 5, 'Data indent should be changeable');
	$w->dataElement('data', 'This is data');
	$w->endTag('doc');
	$w->end();

	checkResult(<<"EOS", 'Turning dataMode on and off whilst writing');
<doc>
 <data>This is data</data>
 <empty></empty>
 <empty />
 <mixed>This is <mixed /> <x>content</x>.</mixed>
     <data>This is data</data>
</doc>
EOS
};

# Verify that DATA_MODE on its own doesn't cause warnings
TEST: {
	initEnv(
		DATA_MODE => 1
	);

	$w->startTag('doc');
	$w->endTag('doc');

	wasNoWarning('DATA_MODE should not cause warnings');
};

# Test DATA_MODE and initial spacing
TEST: {
	initEnv(
		DATA_MODE => 1
	);

	$w->emptyTag('doc');
	$w->end();
	checkResult("<doc />\n", "An empty element with DATA_MODE");
};

# Test DATA_MODE and initial spacing
TEST: {
	initEnv(
		DATA_MODE => 1
	);

	$w->xmlDecl();
	$w->emptyTag('doc');
	$w->end();
	checkResult(<<"EOS", "An empty element with DATA_MODE");
<?xml version="1.0"?>

<doc />
EOS
};

# Test DATA_MODE and initial spacing
TEST: {
	initEnv(
		DATA_MODE => 1,
		DATA_INDENT => 1
	);

	$w->xmlDecl();
	$w->startTag('doc');
	$w->emptyTag('item');
	$w->endTag('doc');
	$w->end();
	checkResult(<<"EOS", "A nested element with DATA_MODE and a declaration");
<?xml version="1.0"?>

<doc>
 <item />
</doc>
EOS
};

# Writing without namespaces should allow colons
TEST: {
	initEnv(NAMESPACES => 0);
	$w->startTag('test:doc', 'x:attr' => 'value');
	$w->endTag('test:doc');

	checkResult('<test:doc x:attr="value"></test:doc>', 'A namespace-less document that uses colons in names');
};

# Test with NEWLINES
TEST: {
	initEnv(NEWLINES => 1);
	$w->startTag('test');
	$w->endTag('test');
	$w->end();

	checkResult("<test\n></test\n>\n", 'Use of the NEWLINES parameter');
};

# Test bad comments
TEST: {
	initEnv();
	expectError("Comment may not contain '-->'", eval {
		$w->comment('A bad comment -->');
	});
};

# Test invadvisible comments
TEST: {
	initEnv();
	$w->comment("Comments shouldn't contain double dashes i.e., --");
	$w->emptyTag('x');
	$w->end();

	ok($warning =~ "Interoperability problem: ", 'Comments with doubled dashes should cause warnings');
};

# Expect to break on mixed content in data mode
TEST: {
	initEnv();
	$w->setDataMode(1);
	$w->startTag('x');
	$w->characters('Text');
	expectError("Mixed content not allowed in data mode: element x", eval {
		$w->startTag('x');
	});
};

# Break with mixed content with emptyTag as well
TEST: {
	initEnv();
	$w->setDataMode(1);
	$w->startTag('x');
	$w->characters('Text');
	expectError("Mixed content not allowed in data mode: element empty", eval {
		$w->emptyTag('empty');
	});
};

# Break with mixed content when the element is written before the characters
TEST: {
	initEnv();
	$w->setDataMode(1);
	$w->startTag('x');
	$w->emptyTag('empty');
	expectError("Mixed content not allowed in data mode: characters", eval {
		$w->characters('Text');
	});
};

# Break if there are two attributes with the same name
TEST: {
	initEnv(NAMESPACES => 0);
	expectError("Two attributes named", eval {
		$w->emptyTag('x', 'a' => 'First', 'a' => 'Second');
	});
};

# Break if there are two attributes with the same namespace-qualified name
TEST: {
	initEnv();
	expectError("Two attributes named", eval {
		$w->emptyTag('x', ['x', 'a'] => 'First', ['x', 'a'] => 'Second');
	});
};

# Succeed if there are two attributes with the same local name, but
# in different namespaces
TEST: {
	initEnv();
	$w->emptyTag('x', ['x', 'a'] => 'First', ['y', 'a'] => 'Second');
	checkResult('<x __NS1:a="First" __NS2:a="Second" xmlns:__NS1="x" xmlns:__NS2="y" />', 'Two attributes with the same local name, but in different namespaces');
};

# Check failure when characters are written outside the document
TEST: {
	initEnv();
	expectError('Attempt to insert characters outside of document element',
		eval {
			$w->characters('This should fail.');
		});
};

# Make sure that closing a tag straight off fails
TEST: {
	initEnv();
	expectError('End tag .* does not close any open element', eval {
		$w->endTag('x');
	});
};

# Use UNSAFE to allow attributes with emptyTag
TEST: {
	initEnv(UNSAFE => 1);
	$w->emptyTag('x', 'xml:space' => 'preserve', ['x', 'y'] => 'z');
	$w->end();
	checkResult("<x xml:space=\"preserve\" __NS1:y=\"z\" xmlns:__NS1=\"x\" />\n", 'Using UNSAFE to bypass the namespace system for emptyTag');
};

# Use UNSAFE to allow attributes with startTag
TEST: {
	initEnv(UNSAFE => 1);
	$w->startTag('sys:element', 'xml:space' => 'preserve', ['x', 'y'] => 'z');
	$w->endTag('sys:element');
	$w->end();
	checkResult("<sys:element xml:space=\"preserve\" __NS1:y=\"z\" xmlns:__NS1=\"x\"></sys:element>\n", 'Using UNSAFE to bypass the namespace system for startTag');
};

# Exercise nesting and namespaces
TEST: {
	initEnv(DATA_MODE => 1, DATA_INDENT => 1);
	$w->startTag(['a', 'element']);
	$w->startTag(['a', 'element']);
	$w->startTag(['b', 'element']);
	$w->startTag(['b', 'element']);
	$w->startTag(['c', 'element']);
	$w->startTag(['d', 'element']);
	$w->endTag(['d', 'element']);
	$w->startTag(['d', 'element']);
	$w->endTag(['d', 'element']);
	$w->endTag(['c', 'element']);
	$w->endTag(['b', 'element']);
	$w->endTag(['b', 'element']);
	$w->endTag(['a', 'element']);
	$w->endTag(['a', 'element']);
	$w->end();

	checkResult(<<"EOS", "Deep-nesting, to exercise prefix management");
<__NS1:element xmlns:__NS1="a">
 <__NS1:element>
  <__NS2:element xmlns:__NS2="b">
   <__NS2:element>
    <__NS3:element xmlns:__NS3="c">
     <__NS4:element xmlns:__NS4="d"></__NS4:element>
     <__NS4:element xmlns:__NS4="d"></__NS4:element>
    </__NS3:element>
   </__NS2:element>
  </__NS2:element>
 </__NS1:element>
</__NS1:element>
EOS
};

# Raw output.
TEST: {
	initEnv(UNSAFE => 1);
	$w->startTag("foo");
	$w->raw("<bar/>");
	$w->endTag("foo");
	$w->end();
	checkResult("<foo><bar/></foo>\n", 'raw() should pass text through without escaping it');
};

# Attempting raw output in safe mode
TEST: {
	initEnv();
	$w->startTag("foo");
	expectError('raw\(\) is only available when UNSAFE is set', eval {
		$w->raw("<bar/>");
	});
}

# Inserting a CDATA section.
TEST: {
	initEnv();
	$w->startTag("foo");
	$w->cdata("cdata testing - test");
	$w->endTag("foo");
	$w->end();
	checkResult("<foo><![CDATA[cdata testing - test]]></foo>\n",
		'cdata() should create CDATA sections');
};

# Inserting CDATA containing CDATA delimeters ']]>'.
TEST: {
	initEnv();
	$w->startTag("foo");
	$w->cdata("This is a CDATA section <![CDATA[text]]>");
	$w->endTag("foo");
	$w->end();
	checkResult("<foo><![CDATA[This is a CDATA section <![CDATA[text]]]]><![CDATA[>]]></foo>\n", 'If a CDATA section would be invalid, it should be split up');
};

# cdataElement().
TEST: {
	initEnv();
	$w->cdataElement("foo", "hello", a => 'b');
	$w->end();
	checkResult(qq'<foo a="b"><![CDATA[hello]]></foo>\n',
		'cdataElement should produce a valid element containing a CDATA section');
};

# Verify that writing characters using CDATA outside of an element fails
TEST: {
	initEnv();
	expectError('Attempt to insert characters outside of document element',
		eval {
			$w->cdata('Test');
		});
};

# Expect to break on mixed content in data mode
TEST: {
	initEnv();
	$w->setDataMode(1);
	$w->startTag('x');
	$w->cdata('Text');
	expectError("Mixed content not allowed in data mode: element x", eval {
		$w->startTag('x');
	});
};

# Break with mixed content when the element is written before the characters
TEST: {
	initEnv();
	$w->setDataMode(1);
	$w->startTag('x');
	$w->emptyTag('empty');
	expectError("Mixed content not allowed in data mode: characters", eval {
		$w->cdata('Text');
	});
};

# Make sure addPrefix-caused clashes are resolved
TEST: {
	initEnv();

	$w->addPrefix('a', '');
	$w->addPrefix('b', '');

	$w->startTag(['a', 'doc']);
	$w->emptyTag(['b', 'elem']);
	$w->endTag(['a', 'doc']);
	$w->end();

	checkResult(<<"EOS", 'Later addPrefix()s should override earlier ones');
<__NS1:doc xmlns:__NS1="a"><elem xmlns="b" /></__NS1:doc>
EOS
};

# addPrefix should work in the middle of a document
TEST: {
	initEnv();

	$w->addPrefix('a', '');
	$w->startTag(['a', 'doc']);

	$w->addPrefix('b', '');
	$w->emptyTag(['b', 'elem']);
	$w->endTag(['a', 'doc']);
	$w->end();

	checkResult(<<"EOS", 'addPrefix should work in the middle of a document');
<doc xmlns="a"><elem xmlns="b" /></doc>
EOS
};

# Verify changing the default namespace
TEST: {
	initEnv(
		DATA_MODE => 1,
		DATA_INDENT => 1
	);

	$w->addPrefix('a', '');

	$w->startTag(['a', 'doc']);

	$w->startTag(['b', 'elem1']);
	$w->emptyTag(['b', 'elem1']);
	$w->emptyTag(['a', 'elem2']);
	$w->endTag(['b', 'elem1']);
	
	$w->addPrefix('b', '');

	$w->startTag(['b', 'elem1']);
	$w->emptyTag(['b', 'elem1']);
	$w->emptyTag(['a', 'elem2']);
	$w->endTag(['b', 'elem1']);
	
	$w->addPrefix('a', '');

	$w->startTag(['b', 'elem1']);
	$w->emptyTag(['b', 'elem1']);
	$w->emptyTag(['a', 'elem2']);
	$w->endTag(['b', 'elem1']);

	$w->endTag(['a', 'doc']);
	$w->end();
	
	checkResult(<<"EOS", 'The default namespace should be modifiable during a document');
<doc xmlns="a">
 <__NS1:elem1 xmlns:__NS1="b">
  <__NS1:elem1 />
  <elem2 />
 </__NS1:elem1>
 <elem1 xmlns="b">
  <elem1 />
  <__NS1:elem2 xmlns:__NS1="a" />
 </elem1>
 <__NS1:elem1 xmlns:__NS1="b">
  <__NS1:elem1 />
  <elem2 />
 </__NS1:elem1>
</doc>
EOS
};

# Verify forcing namespace declarations mid-document
TEST: {
	initEnv(
		DATA_MODE => 1,
		DATA_INDENT => 1
	);

	$w->addPrefix('a', '');

	$w->startTag(['a', 'doc']);

	$w->forceNSDecl('c');
	$w->startTag(['b', 'elem1']);

	$w->emptyTag(['c', 'elem3']);
	$w->emptyTag(['c', 'elem3']);
	$w->emptyTag(['c', 'elem3']);

	$w->endTag(['b', 'elem1']);

	$w->endTag(['a', 'doc']);
	$w->end();
	
	checkResult(<<"EOS", 'Namespace declarations should be forceable mid-document');
<doc xmlns="a">
 <__NS1:elem1 xmlns:__NS1="b" xmlns:__NS2="c">
  <__NS2:elem3 />
  <__NS2:elem3 />
  <__NS2:elem3 />
 </__NS1:elem1>
</doc>
EOS
};

# Verify that PREFIX_MAP's default prefix is not ignored when
#  a document element is from a different namespace
TEST: {
	initEnv(PREFIX_MAP => {'uri:test', ''},
		FORCED_NS_DECLS => ['uri:test']
	);

	$w->emptyTag(['uri:test2', 'document']);

	$w->end();

	checkResult(<<"EOS", 'The default namespace declaration should be present and correct when the document element belongs to a different namespace');
<__NS1:document xmlns:__NS1="uri:test2" xmlns="uri:test" />
EOS
};

# Without namespaces, addPrefix and removePrefix should be safe NOPs
TEST: {
	initEnv(NAMESPACES => 0);

	$w->addPrefix('these', 'arguments', 'are', 'ignored');
	$w->removePrefix('as', 'are', 'these');

	wasNoWarning('Prefix manipulation on a namespace-unaware instance should not warn');
};

# Make sure that getting and setting the output stream behaves as expected
TEST: {
	initEnv();

	my $out = $w->getOutput();

	isnt($out, undef, 'Output for this fixture must be defined');

	$w->setOutput(\*STDERR);
	is($w->getOutput(), \*STDERR, 'Changing output should be reflected in a subsequent get');

	$w->setOutput($out);
	is ($w->getOutput(), $out, 'Changing output back should succeed');

	$w->emptyTag('x');
	$w->end();
	checkResult("<x />\n", 'After changing the output a document should still be generated');
};

# Make sure that undef implies STDOUT for setOutput
TEST: {
	initEnv();

	$w->setOutput();

	is($w->getOutput(), \*STDOUT, 'If no output is given, STDOUT should be used');
};

# Create an ill-formed document using unsafe mode
TEST: {
	initEnv(UNSAFE => 1);

	$w->xmlDecl('us-ascii');
	$w->comment("--");
	$w->characters("Test\n");
	$w->cdata("Test\n");
	$w->doctype('y', undef, '/');
	$w->emptyTag('x');
	$w->end();
	checkResult(<<EOR, 'Unsafe mode should not enforce validity tests.');
<?xml version="1.0" encoding="us-ascii"?>
<!-- -- -->
Test
<![CDATA[Test
]]><!DOCTYPE y SYSTEM "/">
<x />
EOR

};

# Ensure that newlines in attributes are escaped
TEST: {
	initEnv();

	$w->emptyTag('x', 'a' => "A\nB");
	$w->end();

	checkResult("<x a=\"A&#10;B\" />\n", 'Newlines in attribute values should be escaped');
};

# Make sure UTF-8 is written properly
SKIP: {
	skip $unicodeSkipMessage, 2 unless isUnicodeSupported();

	initEnv(ENCODING => 'utf-8', DATA_MODE => 1);

	$w->xmlDecl();
	$w->comment("\$ \x{A3} \x{20AC}");
	$w->startTag('a');
	$w->dataElement('b', '$');
	$w->dataElement('b', "\x{A3}");
	$w->dataElement('b', "\x{20AC}");
	$w->startTag('c');
	$w->cdata(" \$ \x{A3} \x{20AC} ");
	$w->endTag('c');
	$w->endTag('a');
	$w->end();

	checkResult(<<EOR, 'When requested, output should be UTF-8 encoded');
<?xml version="1.0" encoding="utf-8"?>
<!-- \$ \x{C2}\x{A3} \x{E2}\x{82}\x{AC} -->

<a>
<b>\x{24}</b>
<b>\x{C2}\x{A3}</b>
<b>\x{E2}\x{82}\x{AC}</b>
<c><![CDATA[ \$ \x{C2}\x{A3} \x{E2}\x{82}\x{AC} ]]></c>
</a>
EOR
};

# Capture generated XML in a scalar
TEST: {
	my $s;

	$w = new XML::Writer(OUTPUT => \$s);
	$w->emptyTag('x');
	$w->end();

	wasNoWarning('Capturing in a scalar should not cause warnings');
	is($s, "<x />\n", "Output should be stored in a scalar, if one is passed");
};

# Modify the scalar during capture
TEST: {
	my $s;

	$w = new XML::Writer(OUTPUT => \$s);
	$w->startTag('foo', bar => 'baz');
	is($s, "<foo bar=\"baz\">", 'Scalars should be up-to-date during writing');

	$s = '';
	$w->dataElement('txt', 'blah');
	$w->endTag('foo');
	$w->end();

	is($s, "<txt>blah</txt></foo>\n", 'Resetting the scalar should work properly');
};

# Ensure that ENCODING and SCALAR don't cause failure when used together
TEST: {
	my $s;

	ok(eval {$w = new XML::Writer(OUTPUT => \$s,
		ENCODING => 'utf-8'
	);}, 'OUTPUT and ENCODING should not cause failure');
}

# Verify that unknown encodings cause failure
TEST: {
	expectError('encoding', eval {
		initEnv(ENCODING => 'x-unsupported-encoding');
	});
}

# Make sure scalars are built up as UTF-8 (if UTF-8 is passed in)
SKIP: {
	skip $unicodeSkipMessage, 2 unless isUnicodeSupported();

	my $s;

	$w = new XML::Writer(OUTPUT => \$s);

	my $x = 'x';
	utf8::upgrade($x);

	$w->emptyTag($x);
	$w->end();

	ok(utf8::is_utf8($s), 'A storage scalar should preserve utf8-ness');


	undef($s);
	$w = new XML::Writer(OUTPUT => \$s);
	$w->startTag('a');
	$w->dataElement('x', "\$");
	$w->dataElement('x', "\x{A3}");
	$w->dataElement('x', "\x{20AC}");
	$w->endTag('a');
	$w->end();

	is($s, "<a><x>\$</x><x>\x{A3}</x><x>\x{20AC}</x></a>\n",
		'A storage scalar should work with utf8 strings');
}

# Test US-ASCII encoding
SKIP: {
	skip $unicodeSkipMessage, 7 unless isUnicodeSupported();

	initEnv(ENCODING => 'us-ascii', DATA_MODE => 1);

	$w->xmlDecl();
	$w->startTag('a');
	$w->dataElement('x', "\$", 'a' => "\$");
	$w->dataElement('x', "\x{A3}", 'a' => "\x{A3}");
	$w->dataElement('x', "\x{20AC}", 'a' => "\x{20AC}");
	$w->endTag('a');
	$w->end();

	checkResult(<<'EOR', 'US-ASCII support should cover text and attributes');
<?xml version="1.0" encoding="us-ascii"?>

<a>
<x a="$">$</x>
<x a="&#xA3;">&#xA3;</x>
<x a="&#x20AC;">&#x20AC;</x>
</a>
EOR


	# Make sure non-ASCII characters that can't be represented
	#  as references cause failure
	my $text = "\x{A3}";
#	utf8::upgrade($text);

	initEnv(ENCODING => 'us-ascii', DATA_MODE => 1);
	$w->startTag('a');
	$w->cdata('Text');
	expectError('ASCII', eval {
		$w->cdata($text);
	});


	initEnv(ENCODING => 'us-ascii', DATA_MODE => 1);
	$w->startTag('a');
	$w->comment('Text');
	expectError('ASCII', eval {
		$w->comment($text);
	});


	initEnv(ENCODING => 'us-ascii', DATA_MODE => 1);
	expectError('ASCII', eval {
		$w->emptyTag("\x{DC}berpr\x{FC}fung");
	});


	# Make sure Unicode generates warnings when it makes it through
	#  to a US-ASCII-encoded stream
	initEnv(ENCODING => 'us-ascii', DATA_MODE => 1, UNSAFE => 1);
	$w->startTag('a');
	$w->cdata($text);
	$w->endTag('a');
	$w->end();

	$outputFile->flush();
	ok($warning && $warning =~ /does not map to ascii/,
		'Perl IO should warn about non-ASCII characters in output');
	

	initEnv(ENCODING => 'us-ascii', DATA_MODE => 1, UNSAFE => 1);
	$w->startTag('a');
	$w->comment($text);
	$w->endTag('a');
	$w->end();

	$outputFile->flush();
	ok($warning && $warning =~ /does not map to ascii/,
		'Perl IO should warn about non-ASCII characters in output');

}

# Make sure comments are formatted in data mode
TEST: {
	initEnv(DATA_MODE => 1, DATA_INDENT => 1);

	$w->xmlDecl();
	$w->comment("Test");
	$w->comment("Test");
	$w->startTag("x");
	$w->comment("Test 2");
	$w->startTag("y");
	$w->comment("Test 3");
	$w->endTag("y");
	$w->comment("Test 4");
	$w->startTag("y");
	$w->endTag("y");
	$w->endTag("x");
	$w->end();
	$w->comment("Test 5");

	checkResult(<<'EOR', 'Comments should be formatted like elements when in data mode');
<?xml version="1.0"?>
<!-- Test -->
<!-- Test -->

<x>
 <!-- Test 2 -->
 <y>
  <!-- Test 3 -->
 </y>
 <!-- Test 4 -->
 <y></y>
</x>
<!-- Test 5 -->
EOR
}

# Test characters outside the BMP
SKIP: {
	skip $unicodeSkipMessage, 4 unless isUnicodeSupported();

	my $s = "\x{10480}"; # U+10480 OSMANYA LETTER ALEF

	initEnv(ENCODING => 'utf-8');

	$w->dataElement('x', $s);
	$w->end();

	checkResult(<<"EOR", 'Characters outside the BMP should be encoded correctly in UTF-8');
<x>\xF0\x90\x92\x80</x>
EOR

	initEnv(ENCODING => 'us-ascii');

	$w->dataElement('x', $s);
	$w->end();

	checkResult(<<'EOR', 'Characters outside the BMP should be encoded correctly in US-ASCII');
<x>&#x10480;</x>
EOR
}


# Ensure 'ancestor' returns undef beyond the document
TEST: {
	initEnv();

	is($w->ancestor(0), undef, 'With no document, ancestors should be undef');

	$w->startTag('x');
	is($w->ancestor(0), 'x', 'ancestor(0) should return the current element');
	is($w->ancestor(1), undef, 'ancestor should return undef beyond the document');
}

# Don't allow undefined Unicode characters, but do allow whitespace
TEST: {
	# Test characters

	initEnv();

	$w->startTag('x');
	expectError('\u0000', eval {
		$w->characters("\x00");
	});

	initEnv();

	$w->dataElement('x', "\x09\x0A\x0D ");
	$w->end();

	checkResult(<<"EOR", 'Whitespace below \u0020 is valid.');
<x>\x09\x0A\x0D </x>
EOR


	# CDATA

	initEnv();
	$w->startTag('x');
	expectError('\u0000', eval {
		$w->cdata("\x00");
	});

	initEnv();

	$w->startTag('x');
	$w->cdata("\x09\x0A\x0D ");
	$w->endTag('x');
	$w->end();

	checkResult(<<"EOR", 'Whitespace below \u0020 is valid.');
<x><![CDATA[\x09\x0A\x0D ]]></x>
EOR


	# Attribute values

	initEnv();
	expectError('\u0000', eval {
		$w->emptyTag('x', 'a' => "\x00");
	});

	initEnv();
	$w->emptyTag('x', 'a' => "\x09\x0A\x0D ");
	$w->end();

	# Currently, \u000A is escaped. This test is for lack of errors,
	#  not exact serialisation, so change it if necessary.
	checkResult(<<"EOR", 'Whitespace below \u0020 is valid.');
<x a="\x09&#10;\x0D " />
EOR
}

# Unsafe mode should not enforce character validity tests
TEST: {
	initEnv(UNSAFE => 1);

	$w->dataElement('x', "\x00");
	$w->end();
	checkResult(<<"EOR", 'Unsafe mode should not enforce character validity tests');
<x>\x00</x>
EOR

	initEnv(UNSAFE => 1);
	$w->startTag('x');
	$w->cdata("\x00");
	$w->endTag('x');
	$w->end();
	checkResult(<<"EOR", 'Unsafe mode should not enforce character validity tests');
<x><![CDATA[\x00]]></x>
EOR

	initEnv(UNSAFE => 1);
	$w->emptyTag('x', 'a' => "\x00");
	$w->end();
	checkResult(<<"EOR", 'Unsafe mode should not enforce character validity tests');
<x a="\x00" />
EOR
}

# Cover XML declaration encoding cases
TEST: {
	# No declaration unless specified
	initEnv();
	$w->xmlDecl();
	$w->emptyTag('x');
	$w->end();

	checkResult(<<"EOR", 'When no encoding is specified, the declaration should not include one');
<?xml version="1.0"?>
<x />
EOR

	# An encoding specified in the constructor carries across to the declaration
	initEnv(ENCODING => 'us-ascii');
	$w->xmlDecl();
	$w->emptyTag('x');
	$w->end();

	checkResult(<<"EOR", 'If an encoding is specified for the document, it should appear in the declaration');
<?xml version="1.0" encoding="us-ascii"?>
<x />
EOR

	# Anything passed in the xmlDecl call should override
	initEnv(ENCODING => 'us-ascii');
	$w->xmlDecl('utf-8');
	$w->emptyTag('x');
	$w->end();
	checkResult(<<"EOR", 'An encoding passed to xmlDecl should override any other encoding');
<?xml version="1.0" encoding="utf-8"?>
<x />
EOR

	# The empty string should force the omission of the decl
	initEnv(ENCODING => 'us-ascii');
	$w->xmlDecl('');
	$w->emptyTag('x');
	$w->end();
	checkResult(<<"EOR", 'xmlDecl should treat the empty string as instruction to omit the encoding from the declaration');
<?xml version="1.0"?>
<x />
EOR
}


# Free test resources
$outputFile->close() or die "Unable to close temporary file: $!";

1;

__END__
