--TEST--
DB configurable error handler test
--FILE--
<?php

require_once "DB.php";
error_reporting(4095);

function myfunc(&$obj) {
    print "myfunc here, obj=".$obj->toString()."\n";
}
class myclass {
    function myfunc(&$obj) {
	print "myclass::myfunc here, obj=".$obj->toString()."\n";
    }
}
$obj = new myclass;

$dbh = DB::factory("mysql");
print "default: ";
$e = $dbh->raiseError("return testing error");
print $e->toString() . "\n";
$dbh->setErrorHandling(PEAR_ERROR_PRINT);
print "mode=print: ";
$e = $dbh->raiseError("print testing error");
print "\n";
$dbh->setErrorHandling(PEAR_ERROR_TRIGGER);
print "mode=trigger: ";
$e = $dbh->raiseError("trigger testing error");
$dbh->setErrorHandling(PEAR_ERROR_CALLBACK, "myfunc");
print "mode=function callback: ";
$e = $dbh->raiseError("function callback testing error");
$dbh->setErrorHandling(PEAR_ERROR_CALLBACK, array($obj, "myfunc"));
print "mode=object callback: ";
$e = $dbh->raiseError("object callback testing error");

?>
--EXPECT--
default: [db_error: message="DB Error: return testing error" code=0 mode=return level=notice prefix="" prepend="" append="" debug=""]
mode=print: DB Error: print testing error
mode=trigger: <br>
<b>Notice</b>:  DB Error: trigger testing error in <b>PEAR.php</b> on line <b>204</b><br>
mode=function callback: myfunc here, obj=[db_error: message="DB Error: function callback testing error" code=0 mode=callback callback=myfunc prefix="" prepend="" append="" debug=""]
mode=object callback: myclass::myfunc here, obj=[db_error: message="DB Error: object callback testing error" code=0 mode=callback callback=myclass::myfunc prefix="" prepend="" append="" debug=""]
