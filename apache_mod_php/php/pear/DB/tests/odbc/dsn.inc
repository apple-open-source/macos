<?php

switch (ODBC_TYPE) {
    case "solid":
	$nativedsn = "tcp localhost 1313";
        $dsn = "odbc://tcp+localhost+1313";
	$user = $pass = "test";
	break;
    case "iodbc":
        $nativedsn = "test";
        $dsn = "odbc://test";
        $user = $pass = "";
        break;
    default:
	$nativedsn = $dsn = $user = $pass = "";
	break;
}

?>
