--TEST--
BZ2 with files
--SKIPIF--
<?php if (!extension_loaded("bz2")) print "skip"; ?>
--POST--
--GET--
--FILE--
<?php // $Id: with_files.phpt,v 1.1.1.2 2003/03/11 01:09:17 zarzycki Exp $

error_reporting(E_ALL);

$filename = "testfile.bz2";
$str = "This is a test string.\n";
$bz = bzopen($filename, "w");
bzwrite($bz, $str);
bzclose($bz);

$bz = bzopen($filename, "r");
print bzread($bz, 10);
print bzread($bz);
bzclose($bz);
unlink($filename);

--EXPECT--
This is a test string.
