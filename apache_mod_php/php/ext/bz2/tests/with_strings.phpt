--TEST--
BZ2 with strings
--SKIPIF--
<?php if (!extension_loaded("bz2")) print "skip"; ?>
--POST--
--GET--
--FILE--
<?php // $Id: with_strings.phpt,v 1.1.2.2 2003/09/24 02:29:15 iliaa Exp $

error_reporting(E_ALL);

# This FAILS
$blaat = <<<HEREDOC
This is some random data
HEREDOC;

# This Works: (so, is heredoc related)
#$blaat= 'This is some random data';

$blaat2 = bzdecompress(bzcompress($blaat));

$tests = <<<TESTS
 \$blaat === \$blaat2
TESTS;
 include(dirname(__FILE__) . '/../../../tests/quicktester.inc');

--EXPECT--
OK
