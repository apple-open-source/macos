--TEST--
DB_Error/DB_Warning test
--SKIPIF--
<?php if (!include("DB.php")) print "skip"; ?>
--FILE--
<?php // -*- C++ -*-

// Test for: DB.php
// Parts tested: DB_Error, DB_Warning

function test_error_handler($errno, $errmsg, $file, $line, $vars) {
        $errortype = array (
                1   =>  "Error",
                2   =>  "Warning",
                4   =>  "Parsing Error",
                8   =>  "Notice",
                16  =>  "Core Error",
                32  =>  "Core Warning",
                64  =>  "Compile Error",
                128 =>  "Compile Warning",
                256 =>  "User Error",
                512 =>  "User Warning",
                1024=>  "User Notice"
        );
        $prefix = $errortype[$errno];
        $file = basename($file);
        print "\n$prefix: $errmsg in $file on line $line\n";
}

error_reporting(E_ALL);
set_error_handler("test_error_handler");
require_once "DB.php";

print "testing different error codes...\n";
$e = new DB_Error(); print $e->toString()."\n";
$e = new DB_Error("test error"); print $e->toString()."\n";
$e = new DB_Error(DB_OK); print $e->toString()."\n";
$e = new DB_Error(DB_ERROR); print $e->toString()."\n";
$e = new DB_Error(DB_ERROR_SYNTAX); print $e->toString()."\n";
$e = new DB_Error(DB_ERROR_DIVZERO); print $e->toString()."\n";
$e = new DB_Warning(); print $e->toString()."\n";
$e = new DB_Warning("test warning"); print $e->toString()."\n";
$e = new DB_Warning(DB_WARNING_READ_ONLY); print $e->toString()."\n";

print "testing different error modes...\n";
$e = new DB_Error(DB_ERROR, PEAR_ERROR_PRINT); print $e->toString()."\n";
$e = new DB_Error(DB_ERROR_SYNTAX, PEAR_ERROR_TRIGGER);

print "testing different error serverities...\n";
$e = new DB_Error(DB_ERROR_SYNTAX, PEAR_ERROR_TRIGGER, E_USER_NOTICE);
$e = new DB_Error(DB_ERROR_SYNTAX, PEAR_ERROR_TRIGGER, E_USER_WARNING);
$e = new DB_Error(DB_ERROR_SYNTAX, PEAR_ERROR_TRIGGER, E_USER_ERROR);

?>
--GET--
--POST--
--EXPECT--
testing different error codes...
[db_error: message="DB Error: unknown error" code=-1 mode=return level=notice prefix="" info=""]
[db_error: message="DB Error: test error" code=-1 mode=return level=notice prefix="" info=""]
[db_error: message="DB Error: no error" code=1 mode=return level=notice prefix="" info=""]
[db_error: message="DB Error: unknown error" code=-1 mode=return level=notice prefix="" info=""]
[db_error: message="DB Error: syntax error" code=-2 mode=return level=notice prefix="" info=""]
[db_error: message="DB Error: division by zero" code=-13 mode=return level=notice prefix="" info=""]
[db_warning: message="DB Warning: unknown warning" code=-1000 mode=return level=notice prefix="" info=""]
[db_warning: message="DB Warning: test warning" code=0 mode=return level=notice prefix="" info=""]
[db_warning: message="DB Warning: read only" code=-1001 mode=return level=notice prefix="" info=""]
testing different error modes...
DB Error: unknown error[db_error: message="DB Error: unknown error" code=-1 mode=print level=notice prefix="" info=""]

User Notice: DB Error: syntax error in PEAR.php on line 595
testing different error serverities...

User Notice: DB Error: syntax error in PEAR.php on line 595

User Warning: DB Error: syntax error in PEAR.php on line 595

User Error: DB Error: syntax error in PEAR.php on line 595
