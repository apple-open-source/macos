--TEST--
DB_odbc::numRows test
--SKIPIF--
<?php require "skipif.inc"; ?>
--FILE--
<?php
require "connect.inc";
require "mktable.inc";
require "../numrows.inc";
?>
--EXPECT--
1
2
3
4
5
6
2
0
DB Error: unknown error
