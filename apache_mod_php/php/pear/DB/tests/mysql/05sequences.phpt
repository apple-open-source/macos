--TEST--
DB_mysql sequences
--SKIPIF--
<?php require "skipif.inc"; ?>
--FILE--
<?php
require "connect.inc";
require "../sequences.inc";
?>
--EXPECT--
DB Error: no such table
a=1
b=2
b-a=1
c=1
d=1
