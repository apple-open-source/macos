--TEST--
DB_oci8 sequences
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
