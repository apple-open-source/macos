--TEST--
DB_pgsql transaction test
--SKIPIF--
<?php require "skipif.inc"; ?>
--FILE--
<?php
require "connect.inc";
require "mktable.inc";
require "../transactions.inc";
?>
--EXPECT--
after autocommit: bing one ops=0
before commit: bing one two three ops=2
after commit: bing one two three ops=0
before rollback: bing one two three four five ops=2
after rollback: bing one two three ops=0
before autocommit+rollback: bing one two three six seven ops=0
after autocommit+rollback: bing one two three six seven ops=0
testing that select doesn't disturbe opcount: ok
